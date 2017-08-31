#include "wrap/descriptor_set.hpp"

#include "wrap/buffer.hpp"
#include "wrap/buffer_view.hpp"
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

void DescriptorSet::bind(uint32_t binding, uint32_t index_base, vk::ArrayProxy<Buffer const> const& buffers, vk::DescriptorType const& type) const {
  std::vector<vk::DescriptorBufferInfo> infos{};
  for(auto const& buffer : buffers) {
    infos.emplace_back(buffer, 0, buffer.size());
  }

  vk::WriteDescriptorSet info_write{};
  info_write.dstSet = get();
  info_write.dstBinding = binding;
  info_write.dstArrayElement = index_base;
  info_write.descriptorType = type;
  info_write.descriptorCount = uint32_t(infos.size());
  info_write.pBufferInfo = infos.data();
  m_device.updateDescriptorSets({info_write}, 0);
}

void DescriptorSet::bind(uint32_t binding, uint32_t index_base, vk::ArrayProxy<BufferView const> const& views, vk::DescriptorType const& type) const {
  std::vector<vk::DescriptorBufferInfo> infos{};
  for(auto const& view : views) {
    infos.emplace_back(view.buffer(), view.offset(), view.size());
  }

  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = get();
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = index_base;
  descriptorWrite.descriptorType = type;
  descriptorWrite.descriptorCount = uint32_t(infos.size());
  descriptorWrite.pBufferInfo = infos.data();
  m_device.updateDescriptorSets({descriptorWrite}, 0);
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

void DescriptorSet::check(uint32_t binding, uint32_t index_base, uint32_t count) {
  if (binding > m_info.m_info.bindingCount) {
    throw std::runtime_error{"binding out of range"};
  }
  if (index_base > m_info.m_bindings[binding].descriptorCount) {
    throw std::runtime_error{"base index out of range"};
  }
  if (index_base + count > m_info.m_bindings[binding].descriptorCount) {
    throw std::runtime_error{"count out of range"};
  }
}