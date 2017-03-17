#include "ren/database_geometry.hpp"

#include "wrap/device.hpp"
#include "wrap/image.hpp"
#include "texture_loader.hpp"
#include "transferrer.hpp"

GeometryDatabase::GeometryDatabase()
 :Database{}
{}

GeometryDatabase::GeometryDatabase(GeometryDatabase && rhs)
 :GeometryDatabase{}
{
  swap(rhs);
}

GeometryDatabase::GeometryDatabase(Transferrer& transferrer)
 :Database{transferrer}
{
  // find memory type which supports optimal image and specific depth format
  auto type_img = m_device->suitableMemoryType(vk::Format::eD32Sfloat, vk::ImageTiling::eOptimal, vk::MemoryPropertyFlagBits::eDeviceLocal);
  m_allocator = BlockAllocator{*m_device, type_img, 4 * 4 * 3840 * 2160};
}

GeometryDatabase& GeometryDatabase::operator=(GeometryDatabase&& rhs) {
  swap(rhs);
  return *this;
}

// void GeometryDatabase::store(std::string const& tex_path) {
//   auto pix_data = texture_loader::file(tex_path);

//   Image img_new{*m_device, pix_data.extent, pix_data.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst};
//   m_allocator.allocate(img_new);
 
//   m_transferrer->transitionToLayout(img_new, vk::ImageLayout::eShaderReadOnlyOptimal);
//   m_transferrer->uploadImageData(pix_data.ptr(), img_new);

//   Database::store(tex_path, std::move(img_new));  
// }