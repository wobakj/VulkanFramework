#include "geometry.hpp"

#include "wrap/device.hpp"
#include "transferrer.hpp"
#include "wrap/vertex_info.hpp"

#include <iostream>

Geometry::Geometry()
 :m_model{}
 ,m_buffer{}
{}

Geometry::Geometry(Geometry && dev)
 :Geometry{}
{
  swap(dev);
}

Geometry::Geometry(Transferrer& transferrer, vertex_data const& model)
 :m_model{model}
 ,m_buffer{}
{
  // create one buffer to store all data
  m_view_vertices = BufferView{m_model.vertex_num * m_model.vertex_bytes, vk::BufferUsageFlagBits::eVertexBuffer};
  m_view_indices = BufferView{uint32_t(m_model.indices.size() * vertex_data::INDEX.size), vk::BufferUsageFlagBits::eIndexBuffer};
  
  vk::DeviceSize combined_size = m_view_vertices.size() + m_view_indices.size(  );
  m_buffer = Buffer{transferrer.device(), combined_size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst};

  auto mem_type = transferrer.device().findMemoryType(m_buffer.requirements().memoryTypeBits 
                                           , vk::MemoryPropertyFlagBits::eDeviceLocal);
  m_allocator = StaticAllocator(transferrer.device(), mem_type, m_buffer.requirements().size);
  m_allocator.allocate(m_buffer);

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
  std::swap(m_allocator, dev.m_allocator);
  std::swap(m_buffer, dev.m_buffer);
  m_buffer.setAllocator(m_allocator);
  dev.m_buffer.setAllocator(dev.m_allocator);

  std::swap(m_view_vertices, dev.m_view_vertices);
  std::swap(m_view_indices, dev.m_view_indices);
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

VertexInfo Geometry::vertexInfo() const {
  vertex_data::attrib_flag_t attribs;
  for (auto const& offset : m_model.offsets) {
    attribs |= offset.first;
  }

  return attribs_to_vert_info(attribs, true);
}

uint32_t Geometry::numIndices() const {
  return uint32_t(m_model.indices.size());
}

uint32_t Geometry::numVertices() const {
  return m_model.vertex_num;
}
// for drawing from shared index buffer
uint32_t Geometry::indexOffset() const {
  return 0;
}

// for drawing from shared vertex buffer
uint32_t Geometry::vertexOffset() const {
  return 0;
}
