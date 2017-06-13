#include "wrap/sampler.hpp"

#include "wrap/device.hpp"

Sampler::Sampler()
 :WrapperSampler{}
 ,m_device{nullptr}
{}

Sampler::Sampler(Sampler && rhs)
 :WrapperSampler{}
{
  swap(rhs);
}

Sampler::Sampler(Device const& device, vk::Filter const& filter, vk::SamplerAddressMode const& address)
 :Sampler{}
{
  m_device = &device;
  m_info.magFilter = filter;
  m_info.minFilter = filter;
  m_info.maxAnisotropy = 1.0;
  if (filter == vk::Filter::eNearest) {
    m_info.mipmapMode = vk::SamplerMipmapMode::eNearest;
  }
  else {
    m_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
  }
  m_info.addressModeU = address;
  m_info.addressModeV = address;
  m_info.addressModeW = address;
  m_object = device->createSampler(m_info);
}

Sampler::~Sampler() {
  cleanup();
}

void Sampler::destroy() {
  (*m_device)->destroySampler(get());
}

Sampler& Sampler::operator=(Sampler&& rhs) {
  swap(rhs);
  return *this;
}

void Sampler::swap(Sampler& rhs) {
  WrapperSampler::swap(rhs);
  std::swap(m_device, rhs.m_device);
}

void Sampler::writeToSet(vk::DescriptorSet& set, uint32_t binding, vk::DescriptorType const& type, uint32_t index) const {
  vk::DescriptorImageInfo info_img{};
  info_img.sampler = get();
  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = set;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = index;
  descriptorWrite.descriptorType = type;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &info_img;
  (*m_device)->updateDescriptorSets({descriptorWrite}, 0);
}