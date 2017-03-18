#ifndef VERTEX_INFO_HPP
#define VERTEX_INFO_HPP

#include "wrap/vertex_data.hpp"

#include <vulkan/vulkan.hpp>
#include <vector>

class VertexInfo;

VertexInfo attribs_to_vert_info(vertex_data::attrib_flag_t const& active_attributes, bool interleaved = true);

class VertexInfo {
 public:
  VertexInfo();
  VertexInfo(VertexInfo const& rhs);
  VertexInfo(VertexInfo&& rhs);

  void swap(VertexInfo& rhs);

  VertexInfo& operator=(VertexInfo info);

  vk::PipelineVertexInputStateCreateInfo const& get() const;

  operator vk::PipelineVertexInputStateCreateInfo const&() const;

  void setBinding(uint32_t index, uint32_t stride_bytes, vk::VertexInputRate const& rate = vk::VertexInputRate::eVertex) ;

  void setBinding(vk::VertexInputBindingDescription const& binding);

  void setAttribute(uint32_t binding, uint32_t index, vk::Format const& format, uint32_t offset_bytes);
  
  void setAttribute(vk::VertexInputAttributeDescription const& attribute);

 private:
  void updateReferences();

  vk::PipelineVertexInputStateCreateInfo m_info;
  std::vector<vk::VertexInputBindingDescription> m_bindings;
  std::vector<vk::VertexInputAttributeDescription> m_attributes;
};

#endif
