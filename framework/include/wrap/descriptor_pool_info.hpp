#ifndef DESCRIPTOR_POOL_INFO_HPP
#define DESCRIPTOR_POOL_INFO_HPP

#include <vulkan/vulkan.hpp>
#include <vector>

class Shader;

class DescriptorPoolInfo {
 public:
  DescriptorPoolInfo();

  void reserve(Shader const& shader, uint32_t times);
  void reserve(Shader const& shader, uint32_t idx_set, uint32_t times);
  void reserve(vk::DescriptorType const& type, uint32_t times);
  
  operator vk::DescriptorPoolCreateInfo const&() const;

 private:
  void addVec(std::vector<vk::DescriptorPoolSize> const& sizes);
	void addNewSize(vk::DescriptorPoolSize const& size);
	void addSize(vk::DescriptorPoolSize const& size);

  vk::DescriptorPoolCreateInfo m_info;
  std::vector<vk::DescriptorPoolSize> m_sizes;
};

#endif