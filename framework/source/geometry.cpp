#include "geometry.hpp"

#include "wrap/device.hpp"
#include "transferrer.hpp"

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
 ,m_bind_info{}
 ,m_attrib_info{}
 ,m_buffer{}
{}

Geometry::Geometry(Geometry && dev)
 :Geometry{}
{
  swap(dev);
}

Geometry::Geometry(Transferrer& transferrer, vertex_data const& model)
 :m_model{model}
 ,m_bind_info{}
 ,m_attrib_info{}
 ,m_buffer{}
{
  // TODO: support for multiple bindings
  m_bind_info.emplace_back(model_to_bind(model));
  m_attrib_info = model_to_attr(model);
  // create one buffer to store all data
  m_view_vertices = BufferView{m_model.vertex_num * m_model.vertex_bytes};
  m_view_indices = BufferView{uint32_t(m_model.indices.size() * vertex_data::INDEX.size)};
  
  vk::DeviceSize combined_size = m_view_vertices.size() + m_view_indices.size(  );
  m_buffer = Buffer{transferrer.device(), combined_size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst};

  m_memory = Memory{transferrer.device(), m_buffer.requirements(), vk::MemoryPropertyFlagBits::eDeviceLocal};
  m_buffer.bindTo(m_memory);

  m_view_vertices.bindTo(m_buffer);
  // upload vertex data
  transferrer.uploadBufferData(m_model.data.data(), m_view_vertices);
  // upload index data if existant
  if(!model.indices.empty()) {
    m_view_indices.bindTo(m_buffer);
    transferrer.uploadBufferData(m_model.indices.data(), m_view_indices);
  }
}

 Geometry& Geometry::operator=(Geometry&& dev) {
  swap(dev);
  return *this;
 }

 void Geometry::swap(Geometry& dev) {
  std::swap(m_model, dev.m_model);
  std::swap(m_bind_info, dev.m_bind_info);
  std::swap(m_attrib_info, dev.m_attrib_info);
  std::swap(m_memory, dev.m_memory);
  std::swap(m_buffer, dev.m_buffer);
  std::swap(m_view_vertices, dev.m_view_vertices);
  m_buffer.setMemory(m_memory);
  dev.m_buffer.setMemory(dev.m_memory);
  std::swap(m_view_vertices, dev.m_view_vertices);
  std::swap(m_view_indices, dev.m_view_indices);
  m_view_vertices.setBuffer(m_buffer);
  m_view_indices.setBuffer(m_buffer);
 }

vk::Buffer const& Geometry::buffer() const {
  return m_buffer;
}

BufferView const& Geometry::vertices() const {
  return m_view_vertices;
}

BufferView const& Geometry::indices() const {
  return m_view_indices;
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
  return m_view_indices.offset();
}
