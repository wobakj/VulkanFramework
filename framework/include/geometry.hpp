#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include "wrap/buffer.hpp"
#include "wrap/buffer_view.hpp"
#include "wrap/memory.hpp"
#include "wrap/vertex_data.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class Transferrer;

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

  vk::DeviceSize indexOffset() const;
  vk::PipelineVertexInputStateCreateInfo inputInfo() const;
  std::vector<vk::VertexInputBindingDescription> const& bindInfos() const;
  std::vector<vk::VertexInputAttributeDescription> const& attributeInfos() const;
  std::uint32_t numIndices() const;
  std::uint32_t numVertices() const;

 private:
  vertex_data m_model;
  std::vector<vk::VertexInputBindingDescription> m_bind_info;
  std::vector<vk::VertexInputAttributeDescription> m_attrib_info;
  Buffer m_buffer;
  BufferView m_view_vertices;
  BufferView m_view_indices;
  Memory m_memory;
};

#endif