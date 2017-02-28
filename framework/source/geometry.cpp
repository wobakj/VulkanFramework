#include "geometry.hpp"

#include "wrap/device.hpp"

#include <iostream>

vk::VertexInputBindingDescription model_to_bind(vertex_data const& m) {
  vk::VertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = m.vertex_bytes;
  bindingDescription.inputRate = vk::VertexInputRate::eVertex;
  return bindingDescription;  
}

std::vector<vk::VertexInputAttributeDescription> model_to_attr(vertex_data const& model_) {
  std::vector<vk::VertexInputAttributeDescription> attributeDescriptions{};

  int attrib_index = 0;
  for(vertex_data::attribute const& attribute : vertex_data::VERTEX_ATTRIBS) {
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

Geometry::Geometry()
 :m_model{}
 ,m_device{nullptr}
 ,m_bind_info{}
 ,m_attrib_info{}
 ,m_buffer{}
{}

Geometry::Geometry(Geometry && dev)
 :Geometry{}
{
  swap(dev);
}

Geometry::Geometry(Device& device, vertex_data const& model)
 :m_model{model}
 ,m_device{&device}
 ,m_bind_info{}
 ,m_attrib_info{}
 ,m_buffer{}
{
  // TODO: support for multiple bindings
  m_bind_info.emplace_back(model_to_bind(model));
  m_attrib_info = model_to_attr(model);
  // create one buffer to store all data
  vk::DeviceSize combined_size = m_model.vertex_num * m_model.vertex_bytes + uint32_t(m_model.indices.size() * vertex_data::INDEX.size);
  m_buffer = device.createBuffer(combined_size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst);

  m_memory = Memory{device, m_buffer.requirements(), vk::MemoryPropertyFlagBits::eDeviceLocal};
  m_buffer.bindTo(m_memory);

  // upload vertex data
  device.uploadBufferData(m_model.data.data(), m_model.vertex_num * m_model.vertex_bytes, m_buffer);
  // upload index data if existant
  if(!model.indices.empty()) {
    device.uploadBufferData(m_model.indices.data(), m_model.indices.size() * vertex_data::INDEX.size, m_buffer, m_model.vertex_num * m_model.vertex_bytes);
  }
}

 Geometry& Geometry::operator=(Geometry&& dev) {
  swap(dev);
  return *this;
 }

 void Geometry::swap(Geometry& dev) {
  std::swap(m_model, dev.m_model);
  std::swap(m_device, dev.m_device);
  std::swap(m_bind_info, dev.m_bind_info);
  std::swap(m_attrib_info, dev.m_attrib_info);
  std::swap(m_buffer, dev.m_buffer);
  std::swap(m_memory, dev.m_memory);
 }

vk::Buffer const& Geometry::buffer() const {
  return m_buffer;
}

std::vector<vk::VertexInputBindingDescription> const& Geometry::bindInfos() const {
  return m_bind_info;
}

std::vector<vk::VertexInputAttributeDescription> const& Geometry::attributeInfos() const {
  return m_attrib_info;
}

vk::PipelineVertexInputStateCreateInfo Geometry::inputInfo() const {
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.vertexBindingDescriptionCount = std::uint32_t(m_bind_info.size());
  vertexInputInfo.pVertexBindingDescriptions = m_bind_info.data();
  vertexInputInfo.vertexAttributeDescriptionCount = std::uint32_t(m_attrib_info.size());
  vertexInputInfo.pVertexAttributeDescriptions = m_attrib_info.data();
  return vertexInputInfo;
}

std::uint32_t Geometry::numIndices() const {
  return std::uint32_t(m_model.indices.size());
}

std::uint32_t Geometry::numVertices() const {
  return m_model.vertex_num;
}

vk::DeviceSize Geometry::indexOffset() const {
  return m_model.vertex_num * m_model.vertex_bytes;
}
