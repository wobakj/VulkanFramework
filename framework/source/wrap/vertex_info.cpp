#include "wrap/vertex_info.hpp"


VertexInfo::VertexInfo()
 :m_info{}
{}

VertexInfo::VertexInfo(VertexInfo const& rhs)
 :VertexInfo{}
{
  for (auto const& attrib : rhs.m_attributes) {
    setAttribute(attrib);
  }

  for (auto const& binding : rhs.m_bindings) {
    setBinding(binding);
  }
}

VertexInfo::VertexInfo(VertexInfo&& rhs)
 :VertexInfo{}
{
  swap(rhs);
}

void VertexInfo::swap(VertexInfo& rhs) {
  std::swap(m_info, rhs.m_info);
  std::swap(m_bindings, rhs.m_bindings);
  std::swap(m_attributes, rhs.m_attributes);
  updateReferences();
  rhs.updateReferences();
}

VertexInfo& VertexInfo::operator=(VertexInfo info) {
  swap(info);
  return *this;
}

vk::PipelineVertexInputStateCreateInfo const& VertexInfo::get() const {
  return m_info;
}

VertexInfo::operator vk::PipelineVertexInputStateCreateInfo const&() const {
  return get();
}

void VertexInfo::setBinding(uint32_t index, uint32_t stride_bytes, vk::VertexInputRate const& rate) {
  vk::VertexInputBindingDescription binding{};
  binding.binding = index;
  binding.stride = stride_bytes;
  binding.inputRate = rate;
  setBinding(binding);
}

void VertexInfo::setBinding(vk::VertexInputBindingDescription const& binding) {
  bool found = 0;
  for (auto& curr_binding : m_bindings) {
    if (curr_binding.binding == binding.binding) {
      curr_binding = binding;
      found = true;
      break;
    }
  }
  if (!found) {
    m_bindings.emplace_back(binding);
    updateReferences();
  }
}

void VertexInfo::setAttribute(uint32_t binding, uint32_t index, vk::Format const& format, uint32_t offset_bytes) {
  vk::VertexInputAttributeDescription attribute{};
  attribute.binding = binding;
  attribute.location = index;
  attribute.format = format;
  attribute.offset = offset_bytes;
  setAttribute(attribute);
}

void VertexInfo::setAttribute(vk::VertexInputAttributeDescription const& attribute) {
  bool found = 0;
  for (auto& curr_attribute : m_attributes) {
    if (curr_attribute.binding == attribute.binding
     && curr_attribute.location == attribute.location) {
      curr_attribute = attribute;
      found = true;
      break;
    }
  }
  if (!found) {
    m_attributes.emplace_back(attribute);
    updateReferences();
  }
}

void VertexInfo::updateReferences() {
  m_info.vertexBindingDescriptionCount = uint32_t(m_bindings.size());
  m_info.pVertexBindingDescriptions = m_bindings.data();
  m_info.vertexAttributeDescriptionCount = uint32_t(m_attributes.size());
  m_info.pVertexAttributeDescriptions = m_attributes.data();
}