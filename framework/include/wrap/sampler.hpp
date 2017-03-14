#ifndef SAMPLER_HPP
#define SAMPLER_HPP

#include "wrap/wrapper.hpp"

#include <vulkan/vulkan.hpp>

class Device;

using WrapperSampler = Wrapper<vk::Sampler, vk::SamplerCreateInfo>;
class Sampler : public WrapperSampler {
 public:
  Sampler();
  Sampler(Device const& dev, vk::Filter const& filter, vk::SamplerAddressMode const& address);
  Sampler(Sampler && dev);
  Sampler(Sampler const&) = delete;
  ~Sampler();
  
  Sampler& operator=(Sampler const&) = delete;
  Sampler& operator=(Sampler&& dev);

  void swap(Sampler& dev);
  
  void writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::DescriptorType const& type, uint32_t index = 0) const;

 private:
  void destroy() override;

  Device const* m_device;
};

#endif