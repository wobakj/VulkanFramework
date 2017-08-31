#include "wrap/descriptor_set.hpp"

#include "wrap/image.hpp"
#include "wrap/image_view.hpp"

DescriptorSet::DescriptorSet()
 :WrapperDescriptorSet{}
 ,m_device{}
 ,m_pool{}
{}

DescriptorSet::DescriptorSet(DescriptorSet && rhs)
 :DescriptorSet{}
{
  swap(rhs);
}

DescriptorSet::DescriptorSet(vk::Device const& device, vk::DescriptorSet&& set, DescriptorSetLayoutInfo const& info, vk::DescriptorPool const& pool)
 :WrapperDescriptorSet{std::move(set), info}
 ,m_device{device}
 ,m_pool{pool}
{}

DescriptorSet::~DescriptorSet() {
  cleanup();
}

void DescriptorSet::destroy() {
  // do not destroy seperately, freed with pool
  // m_device.freeDescriptorSets(m_pool, {get()});
}

DescriptorSet& DescriptorSet::operator=(DescriptorSet&& rhs) {
  swap(rhs);
  return *this;
}

void DescriptorSet::swap(DescriptorSet& rhs) {
  WrapperDescriptorSet::swap(rhs);
  std::swap(m_device, rhs.m_device);
  std::swap(m_pool, rhs.m_pool);
}

// combined sampler
void DescriptorSet::bind(std::uint32_t binding, uint32_t index, ImageView const& view, vk::Sampler const& sampler) const {
  bind(binding, index, view, is_depth(view.format()) ? vk::ImageLayout::eDepthStencilReadOnlyOptimal : vk::ImageLayout::eShaderReadOnlyOptimal, sampler);
}

void DescriptorSet::bind(std::uint32_t binding, uint32_t index, ImageView const& view, vk::ImageLayout const& layout, vk::Sampler const& sampler) const {
  vk::DescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = layout;
  imageInfo.imageView = view;
  imageInfo.sampler = sampler;

  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = get();
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = index;
  descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  m_device.updateDescriptorSets({descriptorWrite}, 0);
}

// input attachment
void DescriptorSet::bind(std::uint32_t binding, uint32_t index, ImageView const& view, vk::DescriptorType const& type) const {
  vk::ImageLayout layout = vk::ImageLayout::eUndefined;
  // general layout is required for storage images
  if (type == vk::DescriptorType::eStorageImage) {
    layout = vk::ImageLayout::eGeneral;
  }
  else if (type == vk::DescriptorType::eInputAttachment) {
    if (is_depth(view.format())) {
      layout = vk::ImageLayout::eDepthStencilReadOnlyOptimal; 
    }
    else {
      layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }
  }
  else {
    throw std::runtime_error{"type not supported"};
  }
  bind(binding, index, view, layout, type);
}

void DescriptorSet::bind(uint32_t binding, uint32_t index, ImageView const& view, vk::ImageLayout const& layout, vk::DescriptorType const& type) const {
  vk::DescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = layout;
  imageInfo.imageView = view;

  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = get();
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = index;
  descriptorWrite.descriptorType = type;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  m_device.updateDescriptorSets({descriptorWrite}, 0);
}
