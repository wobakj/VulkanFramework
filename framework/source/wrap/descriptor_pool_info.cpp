#include "wrap/descriptor_pool_info.hpp"

#include "wrap/shader.hpp"

DescriptorPoolInfo::DescriptorPoolInfo()
 :m_info{}
{}

void DescriptorPoolInfo::reserve(Shader const& shader, uint32_t times) {
  addVec(to_pool_sizes(shader.info(), times));
  // each set can be allocated times
  m_info.maxSets += times * uint32_t(shader.info().sets.size());    
}

void DescriptorPoolInfo::reserve(Shader const& shader, uint32_t idx_set, uint32_t times) {
  addVec(to_pool_sizes(shader.info().sets.at(idx_set), times));
  // each set can be allocated times
  m_info.maxSets += times;    
}

DescriptorPoolInfo::operator vk::DescriptorPoolCreateInfo const&() const {
  return m_info;
}

void DescriptorPoolInfo::addVec(std::vector<vk::DescriptorPoolSize> const& sizes) {
  for(auto const& size_add : sizes) {
    bool found = false;
    for (auto& size_curr : m_sizes) {
      if (size_curr.type == size_add.type) {
        size_curr.descriptorCount += size_add.descriptorCount;
        found = true;
        break;
      }
    }
    // type not yet contained -> add to vector
    if (!found) {
      m_sizes.emplace_back(size_add);
    }
  }
  // update references if vector was resized
  m_info.poolSizeCount = std::uint32_t(m_sizes.size());
  m_info.pPoolSizes = m_sizes.data();
}