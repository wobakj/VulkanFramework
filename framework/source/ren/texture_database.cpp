#include "ren/texture_database.hpp"

#include "wrap/device.hpp"
#include "wrap/image.hpp"
#include "texture_loader.hpp"
#include "transferrer.hpp"

TextureDatabase::TextureDatabase()
 :Database{}
{}

TextureDatabase::TextureDatabase(TextureDatabase && rhs)
 :TextureDatabase{}
{
  swap(rhs);
}

TextureDatabase::TextureDatabase(Transferrer& transferrer)
 :Database{transferrer}
 ,m_sampler{*m_device, vkDestroySampler}
{
  m_sampler = (*m_device)->createSampler({{}, vk::Filter::eLinear, vk::Filter::eLinear});
  // find memory type which supports optimal image and specific depth format
  auto type_img = m_device->suitableMemoryType(vk::Format::eD32Sfloat, vk::ImageTiling::eOptimal, vk::MemoryPropertyFlagBits::eDeviceLocal);
  m_allocator = BlockAllocator{*m_device, type_img, 4 * 4 * 3840 * 2160};
}

TextureDatabase& TextureDatabase::operator=(TextureDatabase&& rhs) {
  swap(rhs);
  return *this;
}

void TextureDatabase::swap(TextureDatabase& rhs) {
  std::swap(m_indices, m_indices);
  std::swap(m_sampler, m_sampler);
}

void TextureDatabase::store(std::string const& tex_path, Image&& texture) {
  m_indices.emplace(tex_path, m_indices.size());
  Database::store(tex_path, std::move(texture));  
}

void TextureDatabase::store(std::string const& tex_path) {
  auto pix_data = texture_loader::file(tex_path);

  Image img_new{*m_device, pix_data.extent, pix_data.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst};
  m_allocator.allocate(img_new);
 
  m_transferrer->transitionToLayout(img_new, vk::ImageLayout::eShaderReadOnlyOptimal);
  m_transferrer->uploadImageData(pix_data.ptr(), img_new);

  store(tex_path, std::move(img_new));  
}

void TextureDatabase::writeToSet(vk::DescriptorSet& set, std::uint32_t binding) const {
  vk::WriteDescriptorSet descriptorWrite{};
  descriptorWrite.dstSet = set;
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = 0;
  // descriptorWrite.descriptorType = vk::DescriptorType::eSampledImage;
  descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;

  std::vector<vk::DescriptorImageInfo> image_infos(m_indices.size(), vk::DescriptorImageInfo{}); 
  // TODO: support images with non-continous indices 
  for (auto img_index : m_indices) {
    vk::DescriptorImageInfo info{};
    // info.imageLayout = m_info.initialLayout;
    info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    info.imageView = m_resources.at(img_index.first).view();
    info.sampler = m_sampler;
    image_infos[img_index.second] = std::move(info);
  }

  descriptorWrite.descriptorCount = uint32_t(image_infos.size());
  descriptorWrite.pImageInfo = image_infos.data();

  (*m_device)->updateDescriptorSets({descriptorWrite}, 0);
}

size_t TextureDatabase::index(std::string const& name) const {
  return m_indices.at(name);
}
