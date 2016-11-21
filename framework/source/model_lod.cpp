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

const std::size_t LAST_NODE = 64;

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
 ,m_bvh{bvh}
 ,m_size_node{sizeof(serialized_triangle) * m_bvh.get_primitives_per_node()}
{
  // for simplifiction, use one staging buffer per buffer
  m_num_uploads = m_num_nodes;
  // create staging memory and buffers
  for(std::size_t i = 0; i < num_uploads; ++i) {
    m_buffers_stage.emplace_back(device.createBuffer(m_size_node, vk::BufferUsageFlagBits::eTransferSrc));
  }
  auto requirements_stage = m_buffers_stage.front().requirements();
  // per-buffer offset
  auto offset_stage = requirements_stage.alignment * vk::DeviceSize(std::ceil(float(requirements_stage.size) / float(requirements_stage.alignment)));
  requirements_stage.size = requirements_stage.size + offset_stage * (m_num_uploads - 1);
  m_memory_stage = Memory{device, requirements_stage, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};

  for(auto& buffer : m_buffers_stage) {
    buffer.bindTo(m_memory_stage);
  }
  // create drawing memory and buffers
  for(std::size_t i = 0; i < num_nodes; ++i) {
    m_buffers.emplace_back(device.createBuffer(m_size_node, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst));
  }
  auto requirements_draw = m_buffers_stage.front().requirements();
  // per-buffer offset
  auto offset_draw = requirements_draw.alignment * vk::DeviceSize(std::ceil(float(requirements_draw.size) / float(requirements_draw.alignment)));
  requirements_draw.size = requirements_draw.size + offset_draw * (num_uploads - 1);
  m_memory = Memory{device, requirements_draw, vk::MemoryPropertyFlagBits::eDeviceLocal};

  for(auto& buffer : m_buffers) {
    buffer.bindTo(m_memory);
  }

  lamure::ren::lod_stream lod_stream;
  lod_stream.open(path);

  std::cout << "bvh has " << m_bvh.get_num_nodes() << " nodes" << std::endl;
  //   size_t stride_in_bytes = sizeof(serialized_triangle) * bvh.get_primitives_per_node();
  // for (vklod::node_t node_id = 0; node_id < bvh.get_num_nodes(); ++node_id) {
    
  //   //e.g. compute offset to data in lod file for current node
  //   size_t offset_in_bytes = node_id * stride_in_bytes;

  //   std::cout << "node_id " << node_id
  //             << ", depth " << bvh.get_depth_of_node(node_id)
  //             << ", address " << offset_in_bytes 
  //             << ", length " << stride_in_bytes << std::endl;
  // }

  // read data from file
  m_nodes = std::vector<std::vector<float>>(bvh.get_num_nodes(), std::vector<float>(m_size_node / 4, 0.0));
  for(std::size_t i = 0; i < bvh.get_num_nodes() && i < LAST_NODE; ++i) {
    size_t offset_in_bytes = i * m_size_node;
    lod_stream.read((char*)m_nodes[i].data(), offset_in_bytes, m_size_node);
  }

  m_model = model_t{m_nodes.front(), model_t::POSITION | model_t::NORMAL | model_t::TEXCOORD};

  m_bind_info = model_to_bind(m_model);
  m_attrib_info = model_to_attr(m_model);
  // upload initial data
  uint32_t level = 0;
  while(m_num_nodes >= m_bvh.get_length_of_depth(level)) {
    ++level;
  }
  level -= 1;
  std::cout << "drawing level " << level << std::endl;

  std::vector<std::size_t> cut_new{};
  for(std::size_t i = 0; i < m_bvh.get_length_of_depth(level); ++i) {
    cut_new.push_back(m_bvh.get_first_node_id_of_depth(level) + i);
  }

  setCut(cut_new);
}

void ModelLod::nodeToBuffer(std::size_t node, std::size_t buffer) {
  std::cout << "loading node " << node << " into buffer " << buffer << std::endl;
  m_buffers_stage[buffer].setData(m_nodes[node].data(), m_size_node, 0);
  m_device->copyBuffer(m_buffers_stage[buffer], m_buffers[buffer], m_size_node, 0, 0);
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
  std::swap(m_buffers, dev.m_buffers);
  std::swap(m_buffers_stage, dev.m_buffers_stage);
  for (auto& buffer : m_buffers_stage) {
    buffer.setMemory(m_memory_stage);
  }
  for (auto& buffer : m_buffers) {
    buffer.setMemory(m_memory);
  }
  std::swap(m_num_uploads, dev.m_num_uploads);
  std::swap(m_num_nodes, dev.m_num_nodes);
  std::swap(m_nodes, dev.m_nodes);
  std::swap(m_bvh, dev.m_bvh);
  std::swap(m_size_node, dev.m_size_node);
  std::swap(m_cut, dev.m_cut);
  std::swap(m_active_buffers, dev.m_active_buffers);
 }

vk::Buffer const& ModelLod::buffer(std::size_t i) const {
  return m_buffers[i];
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
  return 1.0f / (dist * dist); 
}

bool ModelLod::nodeSplitable(std::size_t node) {
  return m_bvh.get_child_id(node, m_bvh.get_fan_factor()) < LAST_NODE;
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
  // split node woth highest error first
  std::priority_queue<pri_node, std::vector<pri_node>, std::greater<pri_node>> queue_split{};
  // keep node with lowest error first
  std::priority_queue<pri_node, std::vector<pri_node>, std::less<pri_node>> queue_keep{};
  std::set<std::size_t> ignore{};

  const float max_threshold = 1.0f;
  const float min_threshold = 0.1f;

  for (auto const& node : m_cut) {
    if (ignore.find(node) != ignore.end()) continue;

    auto parent = m_bvh.get_parent_id(node);
    bool all_siblings = true;
    for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
      auto curr_sibling = m_bvh.get_child_id(parent, i);
      if (std::find(m_cut.begin(), m_cut.end(), curr_sibling) == m_cut.end()) {
        all_siblings = false;
      }
    }
    // all siblings in cut
    float error_node = nodeError(pos_view, node);
    if (all_siblings) {
      // todo: check frustum intersection case
      // error too large
      if (error_node > max_threshold && nodeSplitable(node)) {
        queue_split.emplace(error_node, node);    
      }
      // error too small
      else if (error_node < min_threshold) {
        // never collapse root
        if (node == 0) continue;
        queue_collapse.emplace(nodeError(pos_view, parent), parent);
        for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
          ignore.emplace(m_bvh.get_child_id(parent, i));
        }
      }
      else {
        queue_keep.emplace(error_node, node);
      }
    }
    // not all siblings in cut
    else {
      if (error_node > max_threshold && nodeSplitable(node)) {
        queue_split.emplace(error_node, node);
      }
      else {
        queue_keep.emplace(error_node, node);
      }
    }
  }
  // create new cut
  std::vector<std::size_t> cut_new;
  // collapse hodes to free space
  while (!queue_collapse.empty()) {
    // m_active_nodes
    cut_new.push_back(queue_collapse.top().node);
    queue_collapse.pop();
  }
  // add keep nodes
  while (!queue_keep.empty()) {
    // m_active_nodes
    cut_new.push_back(queue_keep.top().node);
    queue_keep.pop();
  }

  while (!queue_split.empty()) {
    // split if enough free memory
    if (m_num_nodes - cut_new.size() >= m_bvh.get_fan_factor()) {
      for(std::size_t i = 0; i < m_bvh.get_fan_factor(); ++i) {
        cut_new.push_back(m_bvh.get_child_id(queue_split.top().node, i));
      }
    }
    else {
      cut_new.push_back(queue_split.top().node);
    }
    queue_split.pop();
  }
  assert(cut_new.size() <= m_num_nodes);

  setCut(cut_new);
}

void ModelLod::setCut(std::vector<std::size_t> const& cut) {
  m_active_buffers.clear();
  // collect nodes that were already in previous cut
  std::set<std::size_t> slots_active{};
  for (auto const& node : cut) {
    for (std::size_t i = 0; i < m_cut.size(); ++i) {
      if (m_cut[i] == node) {
        slots_active.emplace(node);
        m_active_buffers.push_back(i);
        break;
      }
    }
  }
  // collect free nodes
  std::queue<std::size_t> slots_free{};
  for (std::size_t i = 0; i < m_num_nodes; ++i) {
    if (slots_active.find(i) == slots_active.end()) {
      slots_free.emplace(i);
    }
  }
  // upload nodes not yet on GPU
  for (auto const& node : cut) {
    if (slots_active.find(node) == slots_active.end()) {
      auto slot = slots_free.front();
      slots_free.pop();
      nodeToBuffer(node, slot);
      m_active_buffers.push_back(slot);
    }
  }
  // store new cut
  m_cut = cut;

  std::cout << "new cut is (";
  for (auto const& node : m_cut) {
    std::cout << node << ", ";
  }
  std::cout << ")" << std::endl;
}
