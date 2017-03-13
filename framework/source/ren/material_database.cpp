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
 ,m_db_texture{*m_transferrer}
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
  uint32_t index_diffuse = 0;
  if (!resource.tex_diffuse.empty()) {
    if (!m_db_texture.contains(resource.tex_diffuse)) {
      m_db_texture.store(resource.tex_diffuse);
    }
    index_diffuse = uint32_t(m_db_texture.index(resource.tex_diffuse));
  }

  gpu_mat_t gpu_mat{resource.vec_diffuse, index_diffuse};
  m_indices.emplace(name, m_indices.size());
  // storge gpu representation
  m_transferrer->uploadBufferData(&gpu_mat, sizeof(gpu_mat_t), m_buffer, (m_indices.size() - 1) * sizeof(gpu_mat_t));

  // store cpu representation
  Database::store(name, std::move(resource));
}

size_t MaterialDatabase::index(std::string const& name) const {
  return m_indices.at(name);
}

void MaterialDatabase::swap(MaterialDatabase& rhs) {
  Database::swap(rhs);
  std::swap(m_indices, rhs.m_indices);
  std::swap(m_allocator, rhs.m_allocator);
  std::swap(m_buffer, rhs.m_buffer);
  std::swap(m_db_texture, rhs.m_db_texture);
}
