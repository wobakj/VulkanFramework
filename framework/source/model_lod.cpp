#include "model_lod.hpp"

#include "device.hpp"

#include <iostream>

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
 ,m_size_node{sizeof(serialized_triangle) * bvh.get_primitives_per_node()}
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

  // read data from file
  m_nodes = std::vector<std::vector<float>>(num_nodes, std::vector<float>(m_size_node / 4, 0.0));
  for(std::size_t i = 0; i < num_nodes; ++i) {
    size_t offset_in_bytes = i * m_size_node;
    lod_stream.read((char*)m_nodes[i].data(), offset_in_bytes, m_size_node);
  }

  m_model = model_t{m_nodes.front(), model_t::POSITION | model_t::NORMAL | model_t::TEXCOORD};

  nodeToBuffer(0,0);

  m_bind_info = model_to_bind(m_model);
  m_attrib_info = model_to_attr(m_model);

  uint32_t level = 0;
  while(m_num_nodes >= m_bvh.get_length_of_depth(level + 1)) {
    ++level;
  }
  level -= 1;
  std::cout << "drawing level " << level << std::endl;

  for(std::size_t i = 0; i < bvh.get_length_of_depth(level); ++i) {
    nodeToBuffer(bvh.get_first_node_id_of_depth(level) + i, i);
    m_active_nodes.push_back(i);
  }
}

void ModelLod::nodeToBuffer(std::size_t node, std::size_t buffer) {
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
  std::swap(m_buffers, dev.m_buffers);
  std::swap(m_buffers_stage, dev.m_buffers_stage);
  std::swap(m_memory, dev.m_memory);
  std::swap(m_memory_stage, dev.m_memory_stage);
  std::swap(m_num_uploads, dev.m_num_uploads);
  std::swap(m_num_nodes, dev.m_num_nodes);
  std::swap(m_nodes, dev.m_nodes);
  std::swap(m_bvh, dev.m_bvh);
  std::swap(m_size_node, dev.m_size_node);
  std::swap(m_active_nodes, dev.m_active_nodes);
 }

vk::Buffer const& ModelLod::buffer(std::size_t i) const {
  return m_buffers[i];
}

std::vector<std::size_t> const& ModelLod::activeNodes() const {
  return m_active_nodes;
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