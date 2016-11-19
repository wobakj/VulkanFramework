#include "model.hpp"

#include "device.hpp"

#include <iostream>

vk::VertexInputBindingDescription model_to_bind(model_t const& m) {
  vk::VertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = m.vertex_bytes;
  bindingDescription.inputRate = vk::VertexInputRate::eVertex;
  return bindingDescription;  
}

std::vector<vk::VertexInputAttributeDescription> model_to_attr(model_t const& model_) {
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

Model::Model()
 :m_model{}
 ,m_device{nullptr}
 ,m_bind_info{}
 ,m_attrib_info{}
 ,m_buffer_vertex{}
 ,m_buffer_index{}
{}

Model::Model(Model && dev)
 :Model{}
{
  swap(dev);
}

Model::Model(Device const& device, model_t const& model)
 :m_model{model}
 ,m_device{&device}
 ,m_bind_info{}
 ,m_attrib_info{}
 ,m_buffer_vertex{}
 ,m_buffer_index{}
{
  m_bind_info = model_to_bind(model);
  m_attrib_info = model_to_attr(model);
  // create one staging buffer for uploads
  vk::DeviceSize max_size = std::max(m_model.vertex_num * m_model.vertex_bytes, uint32_t(m_model.indices.size() * model_t::INDEX.size));

  Buffer buffer_stage{device, max_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent};
  Memory memory_stage = Memory{device, buffer_stage.requirements(), buffer_stage.memFlags()};
  buffer_stage.bindTo(memory_stage);

  m_buffer_vertex = device.createBuffer(m_model.vertex_num * m_model.vertex_bytes, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

  if(!model.indices.empty()) {
    m_buffer_index = device.createBuffer(m_model.indices.size() * model_t::INDEX.size, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
    // if indices are contained, store in same memory
    if (m_buffer_index.memoryType() != m_buffer_vertex.memoryType()) {
      throw std::runtime_error{"memory types dont match"};
    }
    auto combined_requirements = m_buffer_vertex.requirements();
    combined_requirements.size += m_buffer_index.requirements().size;
    m_memory = Memory{device, combined_requirements, m_buffer_vertex.memFlags()};
    m_buffer_vertex.bindTo(m_memory);
    m_buffer_index.bindTo(m_memory);
    // upload index data
    buffer_stage.setData(m_model.indices.data(), m_model.indices.size() * model_t::INDEX.size);
    device.copyBuffer(buffer_stage.get(), m_buffer_index.get(), m_model.indices.size() * model_t::INDEX.size);
  }
  else {
    m_memory = Memory{device, m_buffer_vertex.requirements(), m_buffer_vertex.memFlags()};
    m_buffer_vertex.bindTo(m_memory);
  }
  // upload vertex data
  buffer_stage.setData(m_model.data.data(), m_model.vertex_num * m_model.vertex_bytes);
  device.copyBuffer(buffer_stage.get(), m_buffer_vertex.get(), m_model.vertex_num * m_model.vertex_bytes);
}

 Model& Model::operator=(Model&& dev) {
  swap(dev);
  return *this;
 }

 void Model::swap(Model& dev) {
  std::swap(m_model, dev.m_model);
  std::swap(m_device, dev.m_device);
  std::swap(m_bind_info, dev.m_bind_info);
  std::swap(m_attrib_info, dev.m_attrib_info);
  std::swap(m_buffer_vertex, dev.m_buffer_vertex);
  std::swap(m_buffer_index, dev.m_buffer_index);
  std::swap(m_memory, dev.m_memory);
 }

vk::Buffer const& Model::bufferVertex() const {
  return m_buffer_vertex;
}

vk::Buffer const& Model::bufferIndex() const {
  return m_buffer_index;
}

vk::PipelineVertexInputStateCreateInfo Model::inputInfo() const {
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount = std::uint32_t(m_attrib_info.size());
  vertexInputInfo.pVertexBindingDescriptions = &m_bind_info;
  vertexInputInfo.pVertexAttributeDescriptions = m_attrib_info.data();
  return vertexInputInfo;
}

std::uint32_t Model::numIndices() const {
  return std::uint32_t(m_model.indices.size());
}

vk::DeviceSize Model::indexOffset() const {
  return m_model.vertex_num * m_model.vertex_bytes;
}
