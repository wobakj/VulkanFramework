#include "ren/material_database.hpp"

#include "wrap/device.hpp"
#include "wrap/image.hpp"
#include "wrap/command_buffer.hpp"
#include "texture_loader.hpp"
#include "transferrer.hpp"

#include <utility>

MaterialDatabase::MaterialDatabase()
 :Database{}
{}

MaterialDatabase::MaterialDatabase(MaterialDatabase && rhs)
{
  swap(rhs);
}

MaterialDatabase::MaterialDatabase(Transferrer& transferrer)
 :Database{transferrer}
{
  m_buffer = Buffer{transferrer.device(), sizeof(gpu_mat_t) * 100, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst};

  auto mem_type = transferrer.device().findMemoryType(m_buffer.requirements().memoryTypeBits 
                                           , vk::MemoryPropertyFlagBits::eDeviceLocal);
  m_allocator = StaticAllocator(transferrer.device(), mem_type, m_buffer.requirements().size);
  m_allocator.allocate(m_buffer);
}

MaterialDatabase& MaterialDatabase::operator=(MaterialDatabase&& rhs) {
  swap(rhs);
  return *this;
}

void MaterialDatabase::store(std::string const& name, material_t&& resource) {
  gpu_mat_t gpu_mat{resource.vec_diffuse, 0};
  m_indices.emplace(name, m_views.size());
  // storge gpu representation
  m_views.emplace_back(sizeof(gpu_mat_t), vk::BufferUsageFlagBits::eStorageBuffer);
  m_views.back().bindTo(m_buffer);
  m_transferrer->uploadBufferData(&gpu_mat, m_views.back());

  // store cpu representation
  Database::store(name, std::move(resource));
}

size_t MaterialDatabase::index(std::string const& name) const {
  return m_indices.at(name);
}

void MaterialDatabase::swap(MaterialDatabase& rhs) {
  Database::swap(rhs);
  std::swap(m_indices, rhs.m_indices);
  std::swap(m_views, rhs.m_views);
  std::swap(m_allocator, rhs.m_allocator);
  std::swap(m_buffer, rhs.m_buffer);
}

void MaterialDatabase::updateCommand(CommandBuffer& buffer) const {

}