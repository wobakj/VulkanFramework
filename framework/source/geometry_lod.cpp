#include "geometry_lod.hpp"

#include "wrap/device.hpp"
#include "wrap/vertex_info.hpp"
#include "camera.hpp"
#include "geometry_loader.hpp"
#include "transferrer.hpp"

#include <iostream>
#include <queue>
#include <set>

#define FULL_UPLOAD

template<typename T, typename U>
bool contains(T const& container, U const& element) {
  return std::find(container.begin(), container.end(), element) != container.end();
}

GeometryLod::GeometryLod()
 :m_model{}
 ,m_device{nullptr}
 ,m_ptr_mem_stage{nullptr}
{}

GeometryLod::GeometryLod(GeometryLod && dev)
 :GeometryLod{}
{
  swap(dev);
}

GeometryLod::GeometryLod(Transferrer& transferrer, std::string const& path, std::size_t cut_budget, std::size_t upload_budget)
 :m_model{}
 ,m_device{&transferrer.device()}
 ,m_transferrer{&transferrer}
 ,m_num_nodes{0}
 ,m_num_uploads{0}
 ,m_num_slots{0}
 ,m_bvh{geometry_loader::bvh(path + ".bvh")}
 ,m_size_node{sizeof(serialized_vertex) * m_bvh.get_primitives_per_node()}
 ,m_commands_draw{}
 ,m_ptr_mem_stage{nullptr}
{
  lamure::ren::lod_stream lod_stream;
  lod_stream.open(path + ".lod");

  // read data from file
  m_nodes = std::vector<std::vector<float>>(m_bvh.get_num_nodes(), std::vector<float>(m_size_node * sizeof(uint8_t) / sizeof(float), 0.0));
  for(std::size_t i = 0; i < m_bvh.get_num_nodes(); ++i) {
    size_t offset_in_bytes = i * m_size_node;
    lod_stream.read((char*)m_nodes[i].data(), offset_in_bytes, m_size_node);
  }
  // store model for easier descriptor generation
  m_model = vertex_data{m_nodes.front(), vertex_data::POSITION | vertex_data::NORMAL | vertex_data::TEXCOORD};

  std::cout << "Bvh has depth " << m_bvh.get_depth() << ", with " << m_bvh.get_num_nodes() << " nodes with "  << numVertices() << " vertices each" << std::endl;
// set node buffer sizes
  std::size_t leaf_length = m_bvh.get_length_of_depth(m_bvh.get_depth());
  if (cut_budget > 0) {
    m_num_nodes =  std::max(std::size_t(1), cut_budget * 1024 * 1024 / m_size_node);
  }
  else {
    m_num_nodes = std::max(std::size_t{1}, leaf_length / 3);
  }

  if (upload_budget > 0) {
    m_num_uploads =  std::max(std::size_t(1), upload_budget * 1024 * 1024 / m_size_node);
  }
  else {
    m_num_uploads = std::max(std::size_t{1}, leaf_length / 16);
  }
  // #ifdef FULL_UPLOAD
  //   m_num_slots = m_num_nodes * 2;
  // #else
    m_num_slots = m_num_nodes + m_num_uploads;
  // #endif

  std::cout << "LOD node size is " << m_size_node / 1024 / 1024 << " MB" << std::endl;
  assert(m_num_slots <= m_bvh.get_num_nodes());
  // create staging memory and buffers
  createStagingBuffers();
  // create drawing memory and buffers
  createDrawingBuffers();
  m_commands_draw.resize(m_num_nodes, vk::DrawIndirectCommand{0, 1, 0, 0});

  setFirstCut();
}

void GeometryLod::createStagingBuffers() {
  m_buffer_stage = Buffer{*m_device, m_size_node * m_num_uploads * 2, vk::BufferUsageFlagBits::eTransferSrc};

  auto mem_type = m_device->findMemoryType(m_buffer_stage.requirements().memoryTypeBits 
                                           , vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
  m_allocator_stage = StaticAllocator{*m_device, mem_type, m_buffer_stage.footprint()};
  m_allocator_stage.allocate(m_buffer_stage);

  for(std::size_t i = 0; i < m_num_uploads; ++i) {
    m_db_views_stage.front().emplace_back(BufferRegion{m_buffer_stage, m_size_node, i * m_size_node});
    m_db_views_stage.back().emplace_back(BufferRegion{m_buffer_stage, m_size_node, m_size_node * m_num_uploads + i * m_size_node});
  } 
  // map staging memory once
  m_ptr_mem_stage = m_allocator_stage.map(m_buffer_stage);
}

void GeometryLod::createDrawingBuffers() {
  m_buffer = Buffer{*m_device, m_size_node * m_num_slots, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst};
  auto requirements_draw = m_buffer.requirements();
  // per-buffer offset
  auto offset_draw = requirements_draw.alignment * vk::DeviceSize(std::ceil(float(m_size_node) / float(requirements_draw.alignment)));
  // size of the drawindirect commands
  vk::DeviceSize size_drawbuff = requirements_draw.alignment * vk::DeviceSize(std::ceil(float(sizeof(vk::DrawIndirectCommand) * m_num_slots) / float(requirements_draw.alignment)));
  // size of the level buffer
  vk::DeviceSize size_levelbuff = requirements_draw.alignment * vk::DeviceSize(std::ceil(float(sizeof(float) * (m_num_slots + 1)) / float(requirements_draw.alignment)));
  // total buffer size
  requirements_draw.size = m_size_node + offset_draw * (m_num_slots - 1) + size_drawbuff + size_levelbuff * 2;
  std::cout << "LOD drawing buffer size is " << requirements_draw.size / 1024 / 1024 << " MB for " << m_num_nodes << " nodes" << std::endl;
  m_buffer = Buffer{*m_device, requirements_draw.size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer};

  auto mem_type = m_device->findMemoryType(m_buffer.requirements().memoryTypeBits 
                                           , vk::MemoryPropertyFlagBits::eDeviceLocal);
  m_allocator_draw = StaticAllocator{*m_device, mem_type, m_buffer.footprint()};
  m_allocator_draw.allocate(m_buffer);

  for(std::size_t i = 0; i < m_num_slots; ++i) {
    m_buffer_views.emplace_back(BufferView{m_size_node,vk::BufferUsageFlagBits::eVertexBuffer});
    m_buffer_views.back().bindTo(m_buffer);
  }

  m_view_draw_commands = BufferView{sizeof(vk::DrawIndirectCommand) * m_num_slots, vk::BufferUsageFlagBits::eIndirectBuffer};
  m_view_draw_commands.bindTo(m_buffer);
  m_view_levels = BufferView{sizeof(float) * (m_num_slots + 1), vk::BufferUsageFlagBits::eStorageBuffer};
  m_view_levels.bindTo(m_buffer);
}

void GeometryLod::nodeToSlotImmediate(std::size_t idx_node, std::size_t idx_slot) {
  // get next staging slot
  std::memcpy(m_ptr_mem_stage + m_db_views_stage.back()[0].offset(), m_nodes[idx_node].data(), m_size_node);
  m_transferrer->copyBuffer(m_db_views_stage.back()[0], m_buffer_views[idx_slot]);
  // update slot occupation
  m_slots[idx_slot] = idx_node;
}

void GeometryLod::nodeToSlot(std::size_t idx_node, std::size_t idx_slot) {
  m_node_uploads.emplace_back(idx_node, idx_slot);
  // update slot occupation
  m_slots[idx_slot] = idx_node;
}

std::size_t GeometryLod::numUploads() const {
  return m_node_uploads.size();
}

std::size_t GeometryLod::sizeNode() const {
  return m_size_node;
}

void GeometryLod::performUploads() {
  // auto start = std::chrono::steady_clock::now();
  for(std::size_t i = 0; i < m_node_uploads.size(); ++i) {
    std::size_t idx_node = m_node_uploads[i].first;
    std::memcpy(m_ptr_mem_stage + m_db_views_stage.back()[i].offset(), m_nodes[idx_node].data(), m_size_node);
  }
  // auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start);
  // std::cout << "LOD node copy time: " << time.count() / 1000.0f / 1000.0f << " milliseconds" << std::endl;

  // std::cout << "uploads ";
  // for (auto const& upload : m_node_uploads) {
  //   std::cout << "(" << upload.first << " to " << upload.second << "), ";
  // }
  // std::cout << std::endl;
}

void GeometryLod::performCopies() {
  vk::CommandBuffer const& commandBuffer = m_transferrer->beginSingleTimeCommands();
  performCopiesCommand(commandBuffer);
  m_transferrer->endSingleTimeCommands();

  m_node_uploads.clear();
}

void GeometryLod::performCopiesCommand(vk::CommandBuffer const& command_buffer) {
  if (m_node_uploads.empty()) return;
  // memory transfer to staging is finished
  m_db_views_stage.swap();

  std::vector<vk::BufferCopy> copies_nodes{};
  for(std::size_t i = 0; i < m_node_uploads.size(); ++i) {
    std::size_t idx_slot = m_node_uploads[i].second;
    copies_nodes.emplace_back(m_db_views_stage.front()[i].offset(), m_buffer_views[idx_slot].offset(), m_size_node);
  }
  command_buffer.copyBuffer(m_buffer_stage, m_buffer, copies_nodes);


  std::vector<float> levels(m_num_slots + 1, 0.0f);
  float depth = float(m_bvh.get_depth());
  for (std::size_t i = 0; i < m_num_slots; ++i) {
    levels[i + 1] = float(m_bvh.get_depth_of_node(m_slots[i])) / depth;
  }
  // store number of vertices per node in first entry
  uint32_t verts_per_node = uint32_t(m_buffer_views[1].offset() / m_model.vertex_bytes);
  std::memcpy(levels.data(), &verts_per_node, sizeof(std::uint32_t));
  // upload mode levels
  command_buffer.updateBuffer(
    m_view_levels.buffer(),
    m_view_levels.offset(),
    m_view_levels.size(),
    levels.data()
  );

  // std::cout << "uploading " << m_node_uploads.size() << " nodes with "<< float(m_node_uploads.size() * m_size_node) / 1024.0f / 1024.0f << " MB" << std::endl;
  m_node_uploads.clear();
}

void GeometryLod::updateDrawCommands(vk::CommandBuffer const& command_buffer) {
  // upload draw instructions
  command_buffer.updateBuffer(
    m_view_draw_commands.buffer(),
    m_view_draw_commands.offset(),
    numNodes() * sizeof(vk::DrawIndirectCommand),
    drawCommands().data()
  );
}

void GeometryLod::updateDrawCommands() {
  // std::cout << "drawing slots (";
  for(std::size_t i = 0; i < m_num_nodes; ++i) {
    if (i < m_active_slots.size()) {
      assert(m_buffer_views[0].offset() == 0);
      assert(float(uint32_t(m_buffer_views[m_active_slots[i]].offset() / m_model.vertex_bytes)) == float(m_buffer_views[m_active_slots[i]].offset()) / float(m_model.vertex_bytes));
      assert(m_model.vertex_bytes == sizeof(serialized_vertex));
      assert(m_model.vertex_bytes * numVertices() == m_buffer_views[i].size());
      // assert(m_model.vertex_bytes * numVertices() == m_buffer_views_stage.front().size());
      // assert(m_buffer_views[i].size() == sizeof(serialized_vertex) * m_bvh.get_primitives_per_node());
      assert(m_model.vertex_num == numVertices());
      assert(m_model.vertex_num * m_model.vertex_bytes == m_size_node);
      assert(m_size_node == m_buffer_views[i].size());
      // assert(m_size_node == m_buffer_views_stage.front().size());
      assert(m_buffer.alignment() == m_buffer_stage.alignment());
      m_commands_draw[i].firstVertex = uint32_t(m_buffer_views.at(m_active_slots.at(i)).offset()) / m_model.vertex_bytes;
      // std::cout << "(" << m_active_slots[i] << ", " << m_commands_draw[i].firstVertex << "), ";
      m_commands_draw[i].vertexCount = numVertices();
      m_commands_draw[i].instanceCount = 1;
    }
    else {
      m_commands_draw[i].firstVertex = 0;
      m_commands_draw[i].vertexCount = 0;
      m_commands_draw[i].instanceCount = 0;
    }
  }
  // std::cout << ")" << std::endl;
}

 GeometryLod& GeometryLod::operator=(GeometryLod&& dev) {
  swap(dev);
  return *this;
 }

 void GeometryLod::swap(GeometryLod& dev) {
  std::swap(m_model, dev.m_model);
  std::swap(m_device, dev.m_device);
  std::swap(m_transferrer, dev.m_transferrer);
  std::swap(m_ptr_mem_stage, dev.m_ptr_mem_stage);

  std::swap(m_allocator_stage, dev.m_allocator_stage);
  std::swap(m_allocator_draw, dev.m_allocator_draw);

  std::swap(m_buffer, dev.m_buffer);
  std::swap(m_buffer_stage, dev.m_buffer_stage);
  std::swap(m_buffer_views, dev.m_buffer_views);
  std::swap(m_buffer_views_stage, dev.m_buffer_views_stage);
  std::swap(m_num_uploads, dev.m_num_uploads);
  std::swap(m_num_nodes, dev.m_num_nodes);
  std::swap(m_num_slots, dev.m_num_slots);
  std::swap(m_nodes, dev.m_nodes);
  std::swap(m_bvh, dev.m_bvh);
  std::swap(m_size_node, dev.m_size_node);
  std::swap(m_cut, dev.m_cut);
  std::swap(m_active_slots, dev.m_active_slots);
  std::swap(m_slots, dev.m_slots);
  std::swap(m_node_uploads, dev.m_node_uploads);
  std::swap(m_commands_draw, dev.m_commands_draw);
  
  std::swap(m_view_levels, dev.m_view_levels);
  std::swap(m_view_draw_commands, dev.m_view_draw_commands);

  std::swap(m_db_views_stage, dev.m_db_views_stage);

  updateResourcePointers();
  dev.updateResourcePointers();
}

void GeometryLod::updateResourcePointers() {
  // correct parent resource pointers
  m_buffer.setAllocator(m_allocator_draw);
  m_buffer_stage.setAllocator(m_allocator_stage);
}

BufferView const& GeometryLod::bufferView(std::size_t i) const {
  return m_buffer_views[i];
}

BufferView const& GeometryLod::viewDrawCommands() const {
  return m_view_draw_commands;
}

BufferView const& GeometryLod::viewNodeLevels() const {
  return m_view_levels;
}

Buffer const& GeometryLod::buffer() const {
  return m_buffer;
}

std::size_t GeometryLod::numNodes() const {
  return m_num_nodes;
}

std::vector<std::size_t> const& GeometryLod::cut() const {
  return m_cut;
}

std::vector<std::size_t> const& GeometryLod::activeBuffers() const {
  return m_active_slots;
}

std::vector<vk::DrawIndirectCommand> const& GeometryLod::drawCommands() const {
  return m_commands_draw;
}

VertexInfo GeometryLod::vertexInfo() const {
  vertex_data::attrib_flag_t attribs = 0;
  for (auto const& offset : m_model.offsets) {
    attribs |= offset.first;
  }

  return attribs_to_vert_info(attribs, true);
}

std::uint32_t GeometryLod::numVertices() const {
  return m_model.vertex_num;
}

float GeometryLod::nodeError(glm::fvec3 const& pos_view, std::size_t node) {
  // auto centroid = m_bvh.get_centroid(node);
  auto const& bbox = m_bvh.get_bounding_box(node);
  // (glm::distance(pos_view, glm::fvec3{centroid[0], centroid[1], centroid[2]});
  glm::fvec3 min_dist = glm::fvec3{glm::max(glm::max(bbox.min()[0] - pos_view.x, 0.0), pos_view.x - bbox.max()[0]),
                                   glm::max(glm::max(bbox.min()[1] - pos_view.y, 0.0), pos_view.y - bbox.max()[1]),
                                   glm::max(glm::max(bbox.min()[2] - pos_view.z, 0.0), pos_view.z - bbox.max()[2])};
  // glm::fvec3 bbox_dims = glm::fvec3{glm::abs(bbox.max()[0] - bbox.min()[0]), glm::abs(bbox.max()[1] - bbox.min()[1]), glm::abs(bbox.max()[2] - bbox.min()[2])};
  float level_rel = float(m_bvh.get_depth_of_node(node)) / float(m_bvh.get_depth());
  // return 1.0f / glm::length(min_dist); 
  return  (1.0f - level_rel) / glm::length(min_dist); 
  // return  (1.0f - level_rel) / glm::distance(glm::fvec3{centroid[0], centroid[1], centroid[2]}, pos_view); 
  // return glm::length(bbox_dims) / glm::length(min_dist); 
}

bool GeometryLod::nodeSplitable(std::size_t idx_node) {
  return m_bvh.get_depth_of_node(idx_node) < m_bvh.get_depth() - 1;
}

bool GeometryLod::nodeCollapsible(std::size_t idx_node) {
  return idx_node > 0;
}

struct pri_node {
  pri_node(float err, std::size_t n)
   :error{err}
   ,node{n}
  {}
  float error;
  std::size_t node;

  bool operator<(pri_node const& n) const {
    return error < n.error;
  }
  bool operator>(pri_node const& n) const {
    return error > n.error;
  }
};

void GeometryLod::update(Camera const& cam) {
  update(cam.viewMatrix(), cam.projectionMatrix());
}

void GeometryLod::update(glm::fmat4 const& view, glm::fmat4 const& projection) {
  Frustum2 frustum{};
  frustum.update(projection * view);
  glm::fvec3 position_cam = view[3];
  // collapse to node with lowest error first
  std::priority_queue<pri_node, std::vector<pri_node>, std::less<pri_node>> queue_collapse{};
  // split node with highest error first
  std::priority_queue<pri_node, std::vector<pri_node>, std::greater<pri_node>> queue_split{};
  // keep node with lowest error first
  std::priority_queue<pri_node, std::vector<pri_node>, std::less<pri_node>> queue_keep{};
  std::set<std::size_t> ignore{};

  auto check_duplicate = [queue_keep, queue_split, queue_collapse](std::size_t e) {
    auto q1 = queue_keep;
    auto q2 = queue_collapse;
    auto q3 = queue_split;
    while(!q1.empty()) {
      assert(q1.top().node != e);
      q1.pop();
    }
    while(!q2.empty()) {
      assert(q2.top().node != e);
      q2.pop();
    }
    while(!q3.empty()) {
      assert(q3.top().node != e);
      q3.pop();
    }
  };

  const float max_threshold = 0.05f;
  const float min_threshold = 0.02f;

  for (auto const& idx_node : m_cut) {
    if (ignore.find(idx_node) != ignore.end()) continue;
    float error_node = nodeError(position_cam, idx_node);
    float min_error = error_node;
    // if node is root, is has no siblings
    bool all_siblings = false;
    auto idx_parent = m_bvh.get_parent_id(idx_node);
    if (idx_node > 0) {
      // assume all siblings are in cut
      all_siblings = true;
      for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
        auto idx_sibling = m_bvh.get_child_id(idx_parent, i);
        // make sure no sibling is ignored
        assert(ignore.find(idx_sibling) == ignore.end());
        // check if all siblings lie in cut
        if (!contains(m_cut, idx_sibling)) {
          all_siblings = false;
          break;
        }
      }
      if (all_siblings) {
        // calculate minimal error of all siblings to collapse order independent
        for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
          auto idx_sibling = m_bvh.get_child_id(idx_parent, i);
          min_error = std::min(min_error, nodeError(position_cam, idx_sibling)); 
        }
      }
    }
  // actual cut update
    // error too small or parent ourside frustum
    if (nodeCollapsible(idx_node) && all_siblings && (min_error < min_threshold || !frustum.intersects(m_bvh.get_bounding_box(idx_parent)))) {
      check_duplicate(idx_parent);
      queue_collapse.emplace(nodeError(position_cam, idx_parent), idx_parent);
      // queue_collapse.emplace(nodeError(position_cam, idx_parent) * collapseError(idx_parent), idx_parent);
      for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
        ignore.emplace(m_bvh.get_child_id(idx_parent, i));
      }
    }
    // error too large and within frustum
    // 
    else if (error_node > max_threshold && nodeSplitable(idx_node) && (!nodeCollapsible(idx_node) || frustum.intersects(m_bvh.get_bounding_box(idx_parent)))) {
      check_duplicate(idx_node);
      queue_split.emplace(error_node, idx_node);
    }
    else {
      check_duplicate(idx_node);
      queue_keep.emplace(error_node, idx_node);
    }
  }
// create new cut
  std::vector<std::size_t> cut_new;
  std::size_t num_uploads = 0;
  std::size_t num_new_nodes = 0;
  auto check_sanity = [this, &num_uploads, &cut_new, &num_new_nodes]() {
    for(std::size_t i = 0; i < cut_new.size(); ++i) {
      for(std::size_t j = i + 1; j < cut_new.size(); ++j) {
        assert(cut_new[i] != cut_new[j]);
      }
    }
    assert(num_uploads <= m_num_uploads);
    assert(num_new_nodes <= m_num_uploads);
    assert(cut_new.size() <= m_num_nodes);
  };
  // add keep nodes
  while (!queue_keep.empty()) {
    // keep only if sibling was not collapsed to parent
    auto idx_node = queue_keep.top().node;
    assert(!contains(cut_new, m_bvh.get_parent_id(idx_node)));
    cut_new.push_back(idx_node);
    queue_keep.pop();
    check_sanity();
    // std::cout << "keeping " << idx_node << std::endl;
  }

  // collapse hodes to free space
  while (!queue_collapse.empty()) {
    // m_active_nodes
    auto idx_node = queue_collapse.top().node;
    // new node budget sufficient
    if (num_new_nodes < m_num_uploads) {
      if (!inCore(idx_node)) {
        ++num_uploads;
      }
      cut_new.push_back(idx_node);
      ++num_new_nodes;
    }
    // if budget full, keep children
    else {
      for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
        auto idx_child = m_bvh.get_child_id(idx_node, i);
        // child should be in last cut
        assert(inCore(idx_child));
        cut_new.push_back(idx_child);
      }
    }
    queue_collapse.pop();
    assert(!contains(cut_new, m_bvh.get_parent_id(idx_node)));
    check_sanity();
    // std::cout << "collapsing to " << idx_node << std::endl;
  }

  // split nodes
  while (!queue_split.empty()) {
    bool cancel_split = false;
    // split only if enough memory for remaining nodes
    if (m_num_nodes - cut_new.size() >= m_bvh.get_fan_factor() + queue_split.size() - 1) {
      // check if new nodes are too many for this frame
      if (m_bvh.get_fan_factor() < m_num_uploads - num_new_nodes) {
        for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
          auto idx_child = m_bvh.get_child_id(queue_split.top().node, i);
          cut_new.push_back(idx_child);
          if (!inCore(idx_child)) {
            ++num_uploads;
          }
          ++num_new_nodes;
          check_sanity();
        }
      }
      // too many resulting new nodes from split
      else {
        cancel_split = true;
      }
    }
    // too many resulting draw nodes from split
    else {
      cancel_split = true;
    }
    // fallback
    if (cancel_split) {
      cut_new.push_back(queue_split.top().node);
      check_sanity();
      // std::cout << "dont split " << queue_split.top().node << std::endl;
    }
    assert(!contains(cut_new, m_bvh.get_parent_id(queue_split.top().node)));
    queue_split.pop();
  }

  // printCut();
  setCut(cut_new);
}

float GeometryLod::collapseError(uint64_t idx_node) {
  float child_extent = 0.0f;
  if (nodeSplitable(idx_node)) {
    for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
      child_extent += m_bvh.get_avg_primitive_extent(m_bvh.get_child_id(idx_node, i));
    }
    child_extent /= float(m_bvh.get_fan_factor());
    return m_bvh.get_avg_primitive_extent(idx_node) / child_extent; 
  }
  else return 1.0f;
}

bool GeometryLod::inCore(std::size_t idx_node) {
  return contains(m_slots, idx_node);
}

void GeometryLod::setCut(std::vector<std::size_t> const& cut) {
  auto m_slots_prev = m_slots;
  std::vector<std::size_t> slots_active_new{};
  // collect nodes that were already in previous cut
  std::set<std::size_t> nodes_incore{};
  std::size_t num_reused = 0;
  for (auto const& idx_node : cut) {
    for (std::size_t idx_slot = 0; idx_slot < m_slots.size(); ++idx_slot) {
      if (m_slots.at(idx_slot) == idx_node) {
        #ifdef FULL_UPLOAD
        if (num_reused >= cut.size() - m_num_uploads) {
          break;
        }
        #endif
        // node is incore
        nodes_incore.emplace(idx_node);
        // slot is now active for this cut
        slots_active_new.emplace_back(idx_slot);
        ++num_reused;
        break;
      }
    }
  }
  // collect free nodes
  std::queue<std::size_t> slots_free{};
  for (std::size_t idx_slot = 0; idx_slot < m_num_slots; ++idx_slot) {
    // node is free if it doesnt need to be kept for previous cut
    if (!contains(m_active_slots, idx_slot)
    // and is not active for this cut
     && !contains(slots_active_new, idx_slot)) {
      slots_free.emplace(idx_slot);
    }
  }
  // std::cout << slots_free.size()  << " free slots" << std::endl;
  std::size_t num_uploads = 0; 
  // upload nodes which are not yet on GPU
  for (auto const& idx_node : cut) {
    // node is not yet incore
    if (!contains(nodes_incore, idx_node)) {
      assert(!slots_free.empty());
      // get free slot
      auto idx_slot = slots_free.front();
      slots_free.pop();
      // upload to free slot
      nodeToSlot(idx_node, idx_slot);
      // keep slot during next update
      slots_active_new.emplace_back(idx_slot);
      ++num_uploads;
    }
  }

  // store new cut
  m_cut = cut;
  m_active_slots = slots_active_new;

  assert(nodes_incore.size() <= m_num_nodes);
  assert(m_active_slots.size() <= m_num_nodes);
  assert(num_uploads <= m_num_uploads);
  assert(m_node_uploads.size() <= m_num_uploads);

  if (!m_node_uploads.empty()) {
    performUploads();
    // performCopies();
  }

  updateDrawCommands();
  // printSlots();
}

void GeometryLod::printCut() const {
  std::cout << "cut is (";
  for (auto const& node : m_cut) {
    std::cout << node << ", ";
  }
  std::cout << ")" << std::endl;
  // std::cout << "active slots are (";
  // for (auto const& slots : m_active_slots) {
  //   std::cout << slots << ", ";
  // }
  // std::cout << ")" << std::endl;
}

void GeometryLod::printSlots() const {
  std::cout << "slots are (";
  for (auto const& node : m_slots) {
    std::cout << node << ", ";
  }
  std::cout << ")" << std::endl;
}

void GeometryLod::setFirstCut() {
  uint32_t level = 0;
  while(m_num_nodes >= m_bvh.get_length_of_depth(level) && level < m_bvh.get_depth()) {
    ++level;
  }
  level -= 1;
  std::cout << "initially drawing level " << level << std::endl;

  for(std::size_t i = 0; i < m_bvh.get_length_of_depth(level); ++i) {
    m_cut.push_back(m_bvh.get_first_node_id_of_depth(level) + i);
  }

// set up slot management conainers
  m_slots = std::vector<std::size_t>(m_num_slots, 0);
  // upload nodes which are not yet on GPU
  std::size_t idx_slot = 0;
  for (auto const& idx_node : m_cut) {
    // upload to free slot
    nodeToSlotImmediate(idx_node, idx_slot);
    // keep slot during next update
    m_active_slots.push_back(idx_slot);
    ++idx_slot;
  }
// fill remaining slots
  uint32_t curr_level = level - 1;
  uint32_t idx_local = 0;
  uint64_t first_node = 0;
  if (level > 0) {
    first_node = m_bvh.get_first_node_id_of_depth(curr_level);
    // fill remaining free slots with parent level
    for(; idx_slot < m_num_slots; ++idx_slot) {
      uint64_t idx_node = first_node + idx_local;
      // upload to free slot
      nodeToSlotImmediate(idx_node, idx_slot);
      ++idx_local;
      // go one curr_level down
      if (idx_local >= m_bvh.get_length_of_depth(curr_level)) {
        if (curr_level == 0) {
          break;
        }
        --curr_level;
        first_node = m_bvh.get_first_node_id_of_depth(curr_level);
        idx_local = 0;
      }
    }
  }
  // fill slots with next level
  curr_level = level + 1;
  idx_local = 0;
  first_node = m_bvh.get_first_node_id_of_depth(curr_level);
  for(; idx_slot < m_num_slots; ++idx_slot) {
    uint64_t idx_node = first_node + idx_local;
    // upload to free slot
    nodeToSlotImmediate(idx_node, idx_slot);
    ++idx_local;
    // go one curr_level down
    if (idx_local >= m_bvh.get_length_of_depth(curr_level)) {
      if (curr_level >= m_bvh.get_depth()) {
        throw std::runtime_error{"slots not filled"};
      }
      ++curr_level;
      first_node = m_bvh.get_first_node_id_of_depth(curr_level);
      idx_local = 0;
    }
  }

  // printCut();
  updateDrawCommands();

  // printSlots();
}
