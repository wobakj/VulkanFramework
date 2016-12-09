#include "model_lod.hpp"

#include "device.hpp"

#include <iostream>
#include <queue>
#include <set>

template<typename T, typename U>
bool contains(T const& container, U const& element) {
  return std::find(container.begin(), container.end(), element) != container.end();
}

struct serialized_vertex {
  float v0_x_, v0_y_, v0_z_;   //vertex 0
  float n0_x_, n0_y_, n0_z_;   //normal 0
  float c0_x_, c0_y_;          //texcoord 0
};

static vk::VertexInputBindingDescription model_to_bind(model_t const& m) {
  vk::VertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = m.vertex_bytes;
  bindingDescription.inputRate = vk::VertexInputRate::eVertex;
  return bindingDescription;  
}

static std::vector<vk::VertexInputAttributeDescription> model_to_attr(model_t const& model_) {
  std::vector<vk::VertexInputAttributeDescription> attributeDescriptions{};

  int attrib_index = 0;
  for(model_t::attribute const& attribute : model_t::VERTEX_ATTRIBS) {
    if(model_.offsets.find(attribute) != model_.offsets.end()) {
      vk::VertexInputAttributeDescription desc{};
      desc.binding = 0;
      desc.location = attrib_index;
      desc.format = attribute.type;
      desc.offset = model_.offsets.at(attribute);
      attributeDescriptions.emplace_back(std::move(desc));
      ++attrib_index;
    }
  }
  return attributeDescriptions;  
}

const std::size_t LAST_NODE = 128;

ModelLod::ModelLod()
 :m_model{}
 ,m_device{nullptr}
 ,m_bind_info{}
 ,m_attrib_info{}
{}

ModelLod::ModelLod(ModelLod && dev)
 :ModelLod{}
{
  swap(dev);
}

ModelLod::ModelLod(Device& device, vklod::bvh const& bvh, std::string const& path, std::size_t num_nodes, std::size_t num_uploads)
// ModelLod::ModelLod(Device& device, vklod::bvh const& bvh, lamure::ren::lod_stream&& stream, std::size_t num_nodes, std::size_t num_uploads)
 :m_model{}
 ,m_device{&device}
 ,m_bind_info{}
 ,m_attrib_info{}
 ,m_num_nodes{num_nodes}
 ,m_num_uploads{num_uploads}
 ,m_num_slots{m_num_nodes + num_uploads}
 ,m_bvh{bvh}
 ,m_size_node{sizeof(serialized_vertex) * m_bvh.get_primitives_per_node()}
 ,m_commands_draw(m_num_nodes, vk::DrawIndirectCommand{0, 1, 0, 0})
{
  // create staging memory and buffers
  createStagingBuffers();
  // create drawing memory and buffers
  createDrawingBuffers();

  lamure::ren::lod_stream lod_stream;
  lod_stream.open(path);

  std::cout << "bvh has " << m_bvh.get_num_nodes() << " nodes" << std::endl;

  // read data from file
  m_nodes = std::vector<std::vector<float>>(std::min(bvh.get_num_nodes(), uint32_t(LAST_NODE)), std::vector<float>(m_size_node * sizeof(uint8_t) / sizeof(float), 0.0));
  for(std::size_t i = 0; i < std::min(bvh.get_num_nodes(), uint32_t(LAST_NODE)) && i < LAST_NODE; ++i) {
    size_t offset_in_bytes = i * m_size_node;
    lod_stream.read((char*)m_nodes[i].data(), offset_in_bytes, m_size_node);
  }
  // store model for easier descriptor generation
  m_model = model_t{m_nodes.front(), model_t::POSITION | model_t::NORMAL | model_t::TEXCOORD};
  m_bind_info = model_to_bind(m_model);
  m_attrib_info = model_to_attr(m_model);
  std::cout << numVertices() << std::endl;
  setFirstCut();
}

void ModelLod::createStagingBuffers() {
  m_buffer_stage = m_device->createBuffer(m_size_node * m_num_uploads, vk::BufferUsageFlagBits::eTransferSrc);
  auto requirements_stage = m_buffer_stage.requirements();
  // per-buffer offset
  auto offset_stage = requirements_stage.alignment * vk::DeviceSize(std::ceil(float(m_size_node) / float(requirements_stage.alignment)));
  requirements_stage.size = m_size_node + offset_stage * (m_num_uploads - 1);
  m_buffer_stage = m_device->createBuffer(requirements_stage.size, vk::BufferUsageFlagBits::eTransferSrc);
  m_memory_stage = Memory{*m_device, m_buffer_stage.requirements(), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
  // recreate buffer with correct size, should be calculated beforehand
  m_buffer_stage.bindTo(m_memory_stage);

  for(std::size_t i = 0; i < m_num_uploads; ++i) {
    m_buffer_views_stage.emplace_back(BufferView{m_size_node});
    m_buffer_views_stage.back().bindTo(m_buffer_stage);
  } 
}

void ModelLod::createDrawingBuffers() {
  m_buffer = m_device->createBuffer(m_size_node * m_num_slots, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
  auto requirements_draw = m_buffer.requirements();
  // per-buffer offset
  auto offset_draw = requirements_draw.alignment * vk::DeviceSize(std::ceil(float(m_size_node) / float(requirements_draw.alignment)));
  requirements_draw.size = m_size_node + offset_draw * (m_num_slots - 1);
  m_buffer = m_device->createBuffer(requirements_draw.size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
  m_memory = Memory{*m_device, m_buffer.requirements(), vk::MemoryPropertyFlagBits::eDeviceLocal};
  // ugly, recreate buffer with size for correct allignment
  m_buffer.bindTo(m_memory);

  for(std::size_t i = 0; i < m_num_slots; ++i) {
    m_buffer_views.emplace_back(BufferView{m_size_node});
    m_buffer_views.back().bindTo(m_buffer);
  }
}

void ModelLod::nodeToSlotImmediate(std::size_t idx_node, std::size_t idx_slot) {
  // get next staging slot
  m_buffer_views_stage[0].setData(m_nodes[idx_node].data(), m_size_node, 0);
  m_device->copyBuffer(m_buffer_views_stage[0].buffer(), m_buffer_views[idx_slot].buffer(), m_size_node, m_buffer_views_stage[0].offset(), m_buffer_views[idx_slot].offset());
  // update slot occupation
  m_slots[idx_slot] = idx_node;
}

void ModelLod::nodeToSlot(std::size_t idx_node, std::size_t idx_slot) {
  m_node_uploads.emplace_back(idx_node, idx_slot);
  // update slot occupation
  m_slots[idx_slot] = idx_node;
}

void ModelLod::performUploads() {
  // uint8_t* ptr = (uint8_t*)(m_buffer_stage.map(m_buffer_stage.size(), 0));
  for(std::size_t i = 0; i < m_node_uploads.size(); ++i) {
    std::size_t idx_node = m_node_uploads[i].first;
    // std::memcpy(ptr + m_buffer_views_stage[i].offset(), m_nodes[idx_node].data(), m_size_node);
    m_buffer_views_stage[i].setData(m_nodes[idx_node].data(), m_size_node, 0);
  }
  // m_buffer_stage.unmap();

  std::cout << "uploads ";
  for (auto const& upload : m_node_uploads) {
    std::cout << "(" << upload.first << " to " << upload.second << "), ";
  }
  std::cout << std::endl;
  // assert(0);
}

void ModelLod::performCopies() {
  vk::CommandBuffer const& commandBuffer = m_device->beginSingleTimeCommands();
  performCopiesCommand(commandBuffer);
  m_device->endSingleTimeCommands();

  m_node_uploads.clear();
}

void ModelLod::performCopiesCommand(vk::CommandBuffer const& command_buffer) {
  if (m_node_uploads.empty()) return;

  std::vector<vk::BufferCopy> copies_nodes{};
  for(std::size_t i = 0; i < m_node_uploads.size(); ++i) {
    std::size_t idx_slot = m_node_uploads[i].second;
    copies_nodes.emplace_back(m_buffer_views_stage[i].offset(), m_buffer_views[idx_slot].offset(), m_size_node);
  }
  command_buffer.copyBuffer(m_buffer_stage, m_buffer, copies_nodes);

  m_node_uploads.clear();
}
void ModelLod::updateDrawCommands() {
  // std::cout << "drawing slots (";
  for(std::size_t i = 0; i < m_num_nodes; ++i) {
    if (i < m_active_slots.size()) {
      assert(m_buffer_views[0].offset() == 0);
      assert(float(uint32_t(m_buffer_views[m_active_slots[i]].offset() / m_model.vertex_bytes)) == float(m_buffer_views[m_active_slots[i]].offset()) / float(m_model.vertex_bytes));
      assert(m_model.vertex_bytes == sizeof(serialized_vertex));
      assert(m_model.vertex_bytes * numVertices() == m_buffer_views[i].size());
      assert(m_model.vertex_bytes * numVertices() == m_buffer_views_stage.front().size());
      // assert(m_buffer_views[i].size() == sizeof(serialized_vertex) * m_bvh.get_primitives_per_node());
      assert(m_model.vertex_num == numVertices());
      assert(m_model.vertex_num * m_model.vertex_bytes == m_size_node);
      assert(m_size_node == m_buffer_views[i].size());
      assert(m_size_node == m_buffer_views_stage.front().size());
      assert(m_buffer.alignment() == m_buffer_stage.alignment());
      m_commands_draw[i].firstVertex = uint32_t(m_buffer_views[m_active_slots[i]].offset()) / m_model.vertex_bytes;
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

 ModelLod& ModelLod::operator=(ModelLod&& dev) {
  swap(dev);
  return *this;
 }

 void ModelLod::swap(ModelLod& dev) {
  std::swap(m_model, dev.m_model);
  std::swap(m_device, dev.m_device);
  std::swap(m_bind_info, dev.m_bind_info);
  std::swap(m_attrib_info, dev.m_attrib_info);
  std::swap(m_memory, dev.m_memory);
  std::swap(m_memory_stage, dev.m_memory_stage);
  std::swap(m_buffer, dev.m_buffer);
  std::swap(m_buffer_stage, dev.m_buffer_stage);
  std::swap(m_buffer_views, dev.m_buffer_views);
  std::swap(m_buffer_views_stage, dev.m_buffer_views_stage);
  // corret parent resource pointers
  m_buffer_stage.setMemory(m_memory_stage);
  m_buffer.setMemory(m_memory);
  for (auto& buffer : m_buffer_views_stage) {
    buffer.setBuffer(m_buffer_stage);
  }
  for (auto& buffer : m_buffer_views) {
    buffer.setBuffer(m_buffer);
  }
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
 }

BufferView const& ModelLod::bufferView(std::size_t i) const {
  return m_buffer_views[i];
}

vk::Buffer const& ModelLod::buffer() const {
  return m_buffer;
}

std::size_t ModelLod::numNodes() const {
  return m_num_nodes;
}

std::vector<std::size_t> const& ModelLod::cut() const {
  return m_cut;
}

std::vector<std::size_t> const& ModelLod::activeBuffers() const {
  return m_active_slots;
}

std::vector<vk::DrawIndirectCommand> const& ModelLod::drawCommands() const {
  return m_commands_draw;
}

vk::PipelineVertexInputStateCreateInfo ModelLod::inputInfo() const {
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount = std::uint32_t(m_attrib_info.size());
  vertexInputInfo.pVertexBindingDescriptions = &m_bind_info;
  vertexInputInfo.pVertexAttributeDescriptions = m_attrib_info.data();
  return vertexInputInfo;
}

std::uint32_t ModelLod::numVertices() const {
  return m_model.vertex_num;
}

float ModelLod::nodeError(glm::fvec3 const& pos_view, std::size_t node) {
  auto centroid = m_bvh.get_centroid(node);
  float dist = glm::distance(pos_view, glm::fvec3{centroid[0], centroid[1], centroid[2]});
  auto const& bbox = m_bvh.get_bounding_box(node);
  glm::fvec3 bbox_dims = glm::fvec3{glm::abs(bbox.max()[0] - bbox.min()[0]), glm::abs(bbox.max()[1] - bbox.min()[1]), glm::abs(bbox.max()[2] - bbox.min()[2])};
  return glm::length(bbox_dims) / (dist * dist); 
}

bool ModelLod::nodeSplitable(std::size_t idx_node) {
  return m_bvh.get_depth_of_node(idx_node) < m_bvh.get_depth() - 1 && m_bvh.get_child_id(idx_node, m_bvh.get_fan_factor()) < LAST_NODE;
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

void ModelLod::update(glm::fvec3 const& pos_view) {
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

  const float max_threshold = 0.25f;
  const float min_threshold = 0.05f;

  for (auto const& idx_node : m_cut) {
    if (ignore.find(idx_node) != ignore.end()) continue;
    float error_node = nodeError(pos_view, idx_node) * collapseError(idx_node);
    float min_error = error_node;
    // if node is root, is has no siblings
    bool all_siblings = false;
    auto parent = m_bvh.get_parent_id(idx_node);
    if (idx_node > 0) {
      // assume all siblings are in cut
      all_siblings = true;
      for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
        auto idx_sibling = m_bvh.get_child_id(parent, i);
        // make sure no sibling is ignored
        assert(ignore.find(idx_sibling) == ignore.end());
        // check if all siblings lie in cut
        if (!contains(m_cut, idx_sibling)) {
          all_siblings = false;
          break;
        }
      }
      if (all_siblings) {
        // calculate minimal error of all siblings to collapse be order independent
        for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
          auto idx_sibling = m_bvh.get_child_id(parent, i);
          min_error = std::min(min_error, nodeError(pos_view, idx_sibling) * collapseError(idx_sibling)); 
        }
      }
    }
    // todo: check frustum intersection case
    // error too large
    if (error_node > max_threshold && nodeSplitable(idx_node)) {
      check_duplicate(idx_node);
      queue_split.emplace(error_node, idx_node);
    }
    // error too small
    else if (all_siblings && min_error < min_threshold) {
      check_duplicate(parent);
      queue_collapse.emplace(nodeError(pos_view, parent) * collapseError(parent), parent);
      for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
        ignore.emplace(m_bvh.get_child_id(parent, i));
      }
    }
    else {
      check_duplicate(idx_node);
      queue_keep.emplace(error_node, idx_node);
      // std::cout << "keep " << node << " for missing siblings" << std::endl;
    }
  }

// create new cut
  std::vector<std::size_t> cut_new;
  std::size_t num_uploads = 0;
  auto check_sanity = [this, &num_uploads, &cut_new]() {
    for(std::size_t i = 0; i < cut_new.size(); ++i) {
      for(std::size_t j = i + 1; j < cut_new.size(); ++j) {
        assert(cut_new[i] != cut_new[j]);
      }
    }
    assert(num_uploads <= m_num_uploads);
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
    // if not already in a slot, upload necessary
    if (!inCore(idx_node)) {
      // upload budget sufficient
      if (num_uploads < m_num_uploads) {
        cut_new.push_back(idx_node);
        ++num_uploads;
      }
      // if upload budget full, keep children
      else {
        for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
          auto idx_child = m_bvh.get_child_id(idx_node, i);
          // child should be in last cut
          assert(inCore(idx_child));
          cut_new.push_back(idx_child);
        }
      }
    }
    else {
      cut_new.push_back(idx_node);
    }
    queue_collapse.pop();
    assert(!contains(cut_new, m_bvh.get_parent_id(idx_node)));
    check_sanity();
    // std::cout << "collapsing to " << idx_node << std::endl;
  }

  // split nodes
  while (!queue_split.empty()) {
    // split only if enough memory for remaining nodes
    if (m_num_nodes - cut_new.size() >= m_bvh.get_fan_factor() + queue_split.size() - 1) {
      // collect children which need to be uploaded
      std::size_t num_out_core{};
      for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
        auto idx_child = m_bvh.get_child_id(queue_split.top().node, i);
        if (!inCore(idx_child)) {
          ++num_out_core;
        }
      } 
      // check if split uploads are too many for this frame
      if (num_out_core <= m_num_uploads - num_uploads) {
        for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
          auto idx_child = m_bvh.get_child_id(queue_split.top().node, i);
          cut_new.push_back(idx_child);
          if (!inCore(idx_child)) {
            ++num_uploads;
          }
          check_sanity();
        }
      }
      // too many resulting uploads from split
      else {
        cut_new.push_back(queue_split.top().node);
        check_sanity();
        // std::cout << "dont split " << queue_split.top().node << std::endl;
      }
    }
    // too many resulting nodes from split
    else {
      cut_new.push_back(queue_split.top().node);
      check_sanity();
      // std::cout << "dont split " << queue_split.top().node << std::endl;

    }
    assert(!contains(cut_new, m_bvh.get_parent_id(queue_split.top().node)));
    queue_split.pop();
  }

  printCut();
  setCut(cut_new);
}

float ModelLod::collapseError(uint64_t idx_node) {
  float child_extent = 0.0f;
  for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
    child_extent += m_bvh.get_avg_primitive_extent(m_bvh.get_child_id(idx_node, i));
  }
  child_extent /= float(m_bvh.get_fan_factor());
  return m_bvh.get_avg_primitive_extent(idx_node) / child_extent; 
}

bool ModelLod::inCore(std::size_t idx_node) {
  return contains(m_slots, idx_node);
}

void ModelLod::setCut(std::vector<std::size_t> const& cut) {
  auto m_slots_prev = m_slots;
  std::vector<std::size_t> slots_active_new{};
  // collect nodes that were already in previous cut
  std::set<std::size_t> nodes_incore{};
  for (auto const& idx_node : cut) {
    for (std::size_t idx_slot = 0; idx_slot < m_slots.size(); ++idx_slot) {
      if (m_slots.at(idx_slot) == idx_node) {
        // node is incore
        nodes_incore.emplace(idx_node);
        // slot is now active for this cut
        slots_active_new.emplace_back(idx_slot);
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
  auto change = false;
  for (auto const& idx_node : m_cut) {
    if (!contains(cut, idx_node)) {
      change = true;
      break;
    }
  }
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
  // if (change) {
    updateDrawCommands();
  // }

  // printSlots();
}

void ModelLod::printCut() const {
  std::cout << "cut is (";
  for (auto const& node : m_cut) {
    std::cout << node << ", ";
  }
  std::cout << ")" << std::endl;
  std::cout << "active slots are (";
  for (auto const& slots : m_active_slots) {
    std::cout << slots << ", ";
  }
  std::cout << ")" << std::endl;
}

void ModelLod::printSlots() const {
  std::cout << "slots are (";
  for (auto const& node : m_slots) {
    std::cout << node << ", ";
  }
  std::cout << ")" << std::endl;
}

void ModelLod::setFirstCut() {
  uint32_t level = 0;
  while(m_num_nodes >= m_bvh.get_length_of_depth(level)) {
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

  printCut();
  updateDrawCommands();

  // printSlots();
}
