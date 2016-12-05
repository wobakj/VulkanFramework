#include "model_lod.hpp"

#include "device.hpp"

#include <iostream>
#include <queue>
#include <set>

struct serialized_triangle {
  float v0_x_, v0_y_, v0_z_;   //vertex 0
  float n0_x_, n0_y_, n0_z_;   //normal 0
  float c0_x_, c0_y_;          //texcoord 0
  float v1_x_, v1_y_, v1_z_;
  float n1_x_, n1_y_, n1_z_;
  float c1_x_, c1_y_;
  float v2_x_, v2_y_, v2_z_;
  float n2_x_, n2_y_, n2_z_;
  float c2_x_, c2_y_;
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
 ,m_size_node{sizeof(serialized_triangle) * m_bvh.get_primitives_per_node()}
{
  // create staging memory and buffers
  createStagingBuffers();
  // create drawing memory and buffers
  createDrawingBuffers();

  lamure::ren::lod_stream lod_stream;
  lod_stream.open(path);

  std::cout << "bvh has " << m_bvh.get_num_nodes() << " nodes" << std::endl;

  // read data from file
  m_nodes = std::vector<std::vector<float>>(bvh.get_num_nodes(), std::vector<float>(m_size_node / 4, 0.0));
  for(std::size_t i = 0; i < bvh.get_num_nodes() && i < LAST_NODE; ++i) {
    size_t offset_in_bytes = i * m_size_node;
    lod_stream.read((char*)m_nodes[i].data(), offset_in_bytes, m_size_node);
  }
  // store model for easier descriptor generation
  m_model = model_t{m_nodes.front(), model_t::POSITION | model_t::NORMAL | model_t::TEXCOORD};
  m_bind_info = model_to_bind(m_model);
  m_attrib_info = model_to_attr(m_model);

  setFirstCut();
}

void ModelLod::createStagingBuffers() {
  m_buffer_stage = m_device->createBuffer(m_size_node * m_num_uploads, vk::BufferUsageFlagBits::eTransferSrc);
  for(std::size_t i = 0; i < m_num_uploads; ++i) {
    m_buffer_views_stage.emplace_back(BufferView{m_size_node});
    m_queue_stage.emplace(i);
  }
  auto requirements_stage = m_buffer_stage.requirements();
  // per-buffer offset
  auto offset_stage = requirements_stage.alignment * vk::DeviceSize(std::ceil(float(m_size_node) / float(requirements_stage.alignment)));
  requirements_stage.size = m_size_node + offset_stage * (m_num_uploads - 1);
  m_buffer_stage = m_device->createBuffer(requirements_stage.size, vk::BufferUsageFlagBits::eTransferSrc);
  m_memory_stage = Memory{*m_device, m_buffer_stage.requirements(), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
  // recreate buffer with correct size, should be calculated beforehand
  m_buffer_stage.bindTo(m_memory_stage);

  for(auto& buffer : m_buffer_views_stage) {
    buffer.bindTo(m_buffer_stage);
  } 
}

void ModelLod::createDrawingBuffers() {
  m_buffer = m_device->createBuffer(m_size_node * m_num_slots, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
  for(std::size_t i = 0; i < m_num_slots; ++i) {
    m_buffer_views.emplace_back(BufferView{m_size_node});
  }
  auto requirements_draw = m_buffer.requirements();
  // per-buffer offset
  auto offset_draw = requirements_draw.alignment * vk::DeviceSize(std::ceil(float(m_size_node) / float(requirements_draw.alignment)));
  requirements_draw.size = m_size_node + offset_draw * (m_num_slots - 1);
  m_buffer = m_device->createBuffer(requirements_draw.size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
  m_memory = Memory{*m_device, m_buffer.requirements(), vk::MemoryPropertyFlagBits::eDeviceLocal};
  // ugly, recreate buffer with size for correct allignment
  m_buffer.bindTo(m_memory);

  for(auto& buffer : m_buffer_views) {
    buffer.bindTo(m_buffer);
  }
}

void ModelLod::nodeToSlotImmediate(std::size_t node, std::size_t idx_slot) {
  // get next staging slot
  auto slot_stage = m_queue_stage.front();
  m_queue_stage.pop();
  m_buffer_views_stage[slot_stage].setData(m_nodes[node].data(), m_size_node, 0);
  m_device->copyBuffer(m_buffer_views_stage[slot_stage].buffer(), m_buffer_views[idx_slot].buffer(), m_size_node, m_buffer_views_stage[slot_stage].offset(), m_buffer_views[idx_slot].offset());
  // make staging slot avaible
  m_queue_stage.emplace(slot_stage);
}

void ModelLod::nodeToSlot(std::size_t node, std::size_t idx_slot) {
  m_node_uploads.emplace_back(node, idx_slot);
}

void ModelLod::performUploads() {
  uint8_t* ptr = (uint8_t*)(m_buffer_stage.map(m_node_uploads.size() * m_size_node, 0));
  for(std::size_t i = 0; i < m_node_uploads.size(); ++i) {
    std::size_t idx_node = m_node_uploads[i].first;
    std::memcpy(ptr + m_buffer_views_stage[i].offset(), m_nodes[idx_node].data(), m_size_node);
  }
  m_buffer_stage.unmap();
}

void ModelLod::performCopies() {
  vk::CommandBuffer const& commandBuffer = m_device->beginSingleTimeCommands();
  std::vector<vk::BufferCopy> copies_nodes{};
  for(std::size_t i = 0; i < m_node_uploads.size(); ++i) {
    std::size_t idx_slot = m_node_uploads[i].second;
    copies_nodes.emplace_back(m_buffer_views_stage[i].offset(), m_buffer_views[idx_slot].offset(), m_size_node);
  }
  commandBuffer.copyBuffer(m_buffer_stage, m_buffer, copies_nodes);

  m_device->endSingleTimeCommands();

  m_node_uploads.clear();
}

void ModelLod::performCopiesCommand(vk::CommandBuffer& command_buffer) {
  if (m_node_uploads.empty()) return;

  std::vector<vk::BufferCopy> copies_nodes{};
  for(std::size_t i = 0; i < m_node_uploads.size(); ++i) {
    std::size_t idx_slot = m_node_uploads[i].second;
    copies_nodes.emplace_back(m_buffer_views_stage[i].offset(), m_buffer_views[idx_slot].offset(), m_size_node);
  }
  command_buffer.copyBuffer(m_buffer_stage, m_buffer, copies_nodes);

  m_node_uploads.clear();
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
  std::swap(m_active_buffers, dev.m_active_buffers);
  std::swap(m_slots_keep, dev.m_slots_keep);
  std::swap(m_slots, dev.m_slots);
  std::swap(m_queue_stage, dev.m_queue_stage);
  std::swap(m_node_uploads, dev.m_node_uploads);
 }

BufferView const& ModelLod::bufferView(std::size_t i) const {
  return m_buffer_views[i];
}

vk::Buffer const& ModelLod::buffer() const {
  return m_buffer;
}

std::vector<std::size_t> const& ModelLod::cut() const {
  return m_cut;
}

std::vector<std::size_t> const& ModelLod::activeBuffers() const {
  return m_active_buffers;
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

  const float max_threshold = 0.5f;
  const float min_threshold = 0.05f;

  for (auto const& node : m_cut) {
    if (ignore.find(node) != ignore.end()) continue;
    auto parent = m_bvh.get_parent_id(node);
    bool all_siblings = true;
    for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
      auto curr_sibling = m_bvh.get_child_id(parent, i);
      if (ignore.find(curr_sibling) != ignore.end()) {
        assert(0);
      }
      if (std::find(m_cut.begin(), m_cut.end(), curr_sibling) == m_cut.end()) {
        all_siblings = false;
        break;
      }
    }
    // all siblings in cut
    float error_node = nodeError(pos_view, node) * collapseError(node);
    if (all_siblings) {
      // todo: check frustum intersection case
      // error too large
      if (error_node > max_threshold && nodeSplitable(node)) {
        check_duplicate(node);
        queue_split.emplace(error_node, node);
      }
      // error too small
      else if (error_node < min_threshold) {
        // never collapse root
        if (node == 0)  {
          check_duplicate(node);
          queue_keep.emplace(error_node, node);
          // std::cout << "keep " << node << " for root" << std::endl;
        }
        else {
          check_duplicate(parent);
          queue_collapse.emplace(nodeError(pos_view, parent) * collapseError(parent), parent);
          for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
            ignore.emplace(m_bvh.get_child_id(parent, i));
          }
        }
      }
      else {
        check_duplicate(node);
        queue_keep.emplace(error_node, node);
        // std::cout << "keep " << node << " for missing siblings" << std::endl;
      }
    }
    // not all siblings in cut
    else {
      if (error_node > max_threshold && nodeSplitable(node)) {
        check_duplicate(node);
        queue_split.emplace(error_node, node);
      }
      else {
        check_duplicate(node);
        queue_keep.emplace(error_node, node);
        // std::cout << "keep " << node << " for small error" << std::endl;
      }
    }
  }

// create new cut
  std::vector<std::size_t> cut_new;
  std::size_t num_uploads = 0;
  auto check_sanity = [this, &num_uploads, &cut_new]() {
    for(std::size_t i = 0; i < cut_new.size(); ++i) {
      for(std::size_t j = i + 1; j < cut_new.size(); ++j) {
        if (cut_new[i] == cut_new[j]) {
          assert(0);
        }
      }
    }
    assert(num_uploads <= m_num_uploads);
    assert(cut_new.size() <= m_num_nodes);
  };
  // add keep nodes
  while (!queue_keep.empty()) {
    // keep only if sibling was not collapsed to parent
    auto idx_node = queue_keep.top().node;
    if (std::find(cut_new.begin(), cut_new.end(), m_bvh.get_parent_id(idx_node)) == cut_new.end()) {
      cut_new.push_back(idx_node);
      check_sanity();
    }
    queue_keep.pop();
    check_sanity();
    // std::cout << "keeping " << idx_node << std::endl;
  }

  // collapse hodes to free space
  while (!queue_collapse.empty()) {
    // m_active_nodes
    auto idx_node = queue_collapse.top().node;
    cut_new.push_back(idx_node);
    queue_collapse.pop();
    // if not already in a slot, upload necessary
    if (!inCore(idx_node)) {
      ++num_uploads;
    }
    check_sanity();
    // std::cout << "collapsing to " << idx_node << std::endl;
  }

  // split nodes
  while (!queue_split.empty()) {
    // split only if enough memory for remaining nodes
    if (m_num_nodes - cut_new.size() >= m_bvh.get_fan_factor() + queue_split.size() - 1) {
      // collect children which need to be uploaded
      std::queue<std::size_t> children_out_core{};
      for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
        auto idx_child = m_bvh.get_child_id(queue_split.top().node, i);
        if (!inCore(idx_child)) {
          children_out_core.emplace(idx_child);
        }
      } 
      // check if split uploads are too many for this frame
      if (children_out_core.size() <= m_num_uploads - num_uploads) {
        while(!children_out_core.empty()) {
          cut_new.push_back(children_out_core.front());
          children_out_core.pop();
          ++num_uploads;
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
  return std::find(m_slots.begin(), m_slots.end(), idx_node) != m_slots.end();
}

void ModelLod::setCut(std::vector<std::size_t> const& cut) {
  m_active_buffers.clear();
  std::set<std::size_t> slots_keep_new{};
  // collect nodes that were already in previous cut
  std::set<std::size_t> nodes_incore{};
  for (auto const& idx_node : cut) {
    for (std::size_t idx_slot = 0; idx_slot < m_slots.size(); ++idx_slot) {
      if (m_slots[idx_slot] == idx_node) {
        // slot is now active for this cut
        nodes_incore.emplace(idx_node);
        m_active_buffers.push_back(idx_slot);
        // keep slot during next update
        slots_keep_new.emplace(idx_slot);
        // update slot occupation
        m_slots[idx_slot] = idx_node;
        break;
      }
    }
  }
  // collect free nodes
  std::queue<std::size_t> slots_free{};
  for (std::size_t idx_slot = 0; idx_slot < m_num_slots; ++idx_slot) {
    // node is free if it doesnt need to be kept
    if (m_slots_keep.find(idx_slot) == m_slots_keep.end()
    // and is not active for this cut
     && slots_keep_new.find(idx_slot) == slots_keep_new.end()) {
      slots_free.emplace(idx_slot);
    }
  }
  // std::cout << slots_free.size()  << " free slots" << std::endl;
  std::size_t num_uploads = 0; 
  // upload nodes which are not yet on GPU
  for (auto const& idx_node : cut) {
    // node is not yet in active slot
    if (nodes_incore.find(idx_node) == nodes_incore.end()) {
      assert(!slots_free.empty());
      // get free slot
      auto idx_slot = slots_free.front();
      slots_free.pop();
      // upload to free slot
      nodeToSlot(idx_node, idx_slot);
      m_active_buffers.push_back(idx_slot);
      // keep slot during next update
      slots_keep_new.emplace(idx_slot);
      // update slot occupation
      m_slots[idx_slot] = idx_node;
      ++num_uploads;
    }
  }
  // 
  // store new cut
  m_cut = cut;
  // update slots to keep
  m_slots_keep = slots_keep_new;
  assert(m_slots_keep.size() <= m_num_nodes);
  assert(nodes_incore.size() <= m_num_nodes);
  assert(m_active_buffers.size() <= m_num_nodes);
  assert(num_uploads <= m_num_uploads);
  assert(m_node_uploads.size() <= m_num_uploads);

  if (!m_node_uploads.empty()) {
    performUploads();
  }

  printSlots();
}

void ModelLod::printCut() const {
  std::cout << "cut is (";
  for (auto const& node : m_cut) {
    std::cout << node << ", ";
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
    m_active_buffers.push_back(idx_slot);
    // keep slot during next update
    m_slots_keep.emplace(idx_slot);
    // update slot occupation
    m_slots[idx_slot] = idx_node;
    ++idx_slot;
  }
  // fill remaining free slots with parent level
  uint32_t curr_level = level - 1;
  uint32_t i = 0;
  uint64_t first_node = m_bvh.get_first_node_id_of_depth(curr_level);
  for(; idx_slot < m_num_slots; ++ idx_slot) {
    uint64_t idx_node = first_node + i;
    // upload to free slot
    nodeToSlotImmediate(idx_node, idx_slot);

    // update slot occupation
    m_slots[idx_slot] = idx_node;
    ++i;
    // go one curr_level down
    if (i >= m_bvh.get_length_of_depth(curr_level)) {
      if (curr_level == 0) {
        break;
      }
      --curr_level;
      first_node = m_bvh.get_first_node_id_of_depth(curr_level);
      i = 0;
    }
  }
  // upload slots woth next level
  curr_level = level + 1;
  i = 0;
  first_node = m_bvh.get_first_node_id_of_depth(curr_level);
  for(; idx_slot < m_num_slots; ++ idx_slot) {
    uint64_t idx_node = first_node + i;
    // upload to free slot
    nodeToSlotImmediate(idx_node, idx_slot);
    // update slot occupation
    m_slots[idx_slot] = idx_node;
    ++i;
    // go one curr_level down
    if (i >= m_bvh.get_length_of_depth(curr_level)) {
      ++curr_level;
      if (curr_level > m_bvh.get_depth()) {
        throw std::runtime_error{"slots not filled"};
      }
      first_node = m_bvh.get_first_node_id_of_depth(curr_level);
      i = 0;
    }
  }

  printCut();
  printSlots();
}
