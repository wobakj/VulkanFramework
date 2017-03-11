#include "ren/model_database.hpp"

#include "wrap/device.hpp"
#include "wrap/image.hpp"
#include "texture_loader.hpp"
#include "transferrer.hpp"

ModelDatabase::ModelDatabase()
 :Database{}
{}

ModelDatabase::ModelDatabase(ModelDatabase && rhs)
 :ModelDatabase{}
{
  swap(rhs);
}

ModelDatabase::ModelDatabase(Transferrer& transferrer)
 :Database{transferrer}
{
  // find memory type which supports optimal image and specific depth format
  auto type_img = m_device->suitableMemoryType(vk::Format::eD32Sfloat, vk::ImageTiling::eOptimal, vk::MemoryPropertyFlagBits::eDeviceLocal);
  m_allocator = BlockAllocator{*m_device, type_img, 4 * 4 * 3840 * 2160};
}

ModelDatabase& ModelDatabase::operator=(ModelDatabase&& rhs) {
  swap(rhs);
  return *this;
}

// void ModelDatabase::store(std::string const& tex_path) {
//   auto pix_data = texture_loader::file(tex_path);

//   Image img_new{*m_device, pix_data.extent, pix_data.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst};
//   m_allocator.allocate(img_new);
 
//   m_transferrer->transitionToLayout(img_new, vk::ImageLayout::eShaderReadOnlyOptimal);
//   m_transferrer->uploadImageData(pix_data.ptr(), img_new);

//   Database::store(tex_path, std::move(img_new));  
// }