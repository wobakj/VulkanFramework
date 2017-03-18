#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include "wrap/buffer.hpp"
#include "wrap/buffer_view.hpp"
#include "wrap/vertex_data.hpp"
#include "allocator_static.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Transferrer;
class VertexInfo;

class Geometry {
 public:  
  Geometry();
  Geometry(Transferrer& transferrer, vertex_data const& model);
  Geometry(Geometry && dev);
  Geometry(Geometry const&) = delete;

  Geometry& operator=(Geometry const&) = delete;
  Geometry& operator=(Geometry&& dev);

  void swap(Geometry& dev);

  vk::Buffer const& buffer() const;
  BufferView const& vertices() const;
  BufferView const& indices() const;

  VertexInfo vertexInfo() const;
  uint32_t numIndices() const;
  uint32_t numVertices() const;
  uint32_t indexOffset() const;
  uint32_t vertexOffset() const;

 private:
  vertex_data m_model;
  // destroy allocator after objects
  StaticAllocator m_allocator;
  Buffer m_buffer;
  BufferView m_view_vertices;
  BufferView m_view_indices;
};

#endif