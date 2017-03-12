#include "ren/transform_database.hpp"

#include "wrap/device.hpp"
#include "wrap/image.hpp"
#include "texture_loader.hpp"
#include "transferrer.hpp"

#include <utility>

TransformDatabase::TransformDatabase()
 :Database{}
{}

TransformDatabase::TransformDatabase(TransformDatabase && rhs)
{
  swap(rhs);
}

TransformDatabase::TransformDatabase(Device const& device)
 :Database{}
{
  m_device = & device;
  m_buffer_stage = Buffer{*m_device, sizeof(glm::fmat4) * 100, vk::BufferUsageFlagBits::eTransferSrc};
  auto mem_type = m_device->findMemoryType(m_buffer.requirements().memoryTypeBits 
                                           , vk::MemoryPropertyFlagBits::eDeviceLocal);
  m_allocator = StaticAllocator(*m_device, mem_type, m_buffer.requirements().size);
  m_allocator.allocate(m_buffer);
  m_buffer = Buffer{*m_device, sizeof(glm::fmat4) * 100, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};

  mem_type = m_device->findMemoryType(m_buffer_stage.requirements().memoryTypeBits 
                                           , vk::MemoryPropertyFlagBits::eHostVisible
                                           | vk::MemoryPropertyFlagBits::eHostCoherent);
  m_allocator_stage = StaticAllocator(*m_device, mem_type, m_buffer_stage.requirements().size);
  m_allocator_stage.allocate(m_buffer_stage);

  m_ptr_mem_stage = static_cast<uint8_t*>(m_buffer_stage.map());
}

TransformDatabase& TransformDatabase::operator=(TransformDatabase&& rhs) {
  swap(rhs);
  return *this;
}

void TransformDatabase::store(std::string const& name, glm::fmat4&& resource) {
  m_indices.emplace(name, m_views.size());
  // storge gpu representation
  m_views.emplace_back(sizeof(glm::fmat4), vk::BufferUsageFlagBits::eUniformBuffer);
  m_views.back().bindTo(m_buffer);
  // m_transferrer->uploadBufferData(&gpu_mat, m_views.back());
  set(name, resource);
  // store cpu representation
  // Database::store(name, std::move(resource));
}

size_t TransformDatabase::index(std::string const& name) const {
  return m_indices.at(name);
}

void TransformDatabase::swap(TransformDatabase& rhs) {
  Database::swap(rhs);
  std::swap(m_indices, rhs.m_indices);
  std::swap(m_views, rhs.m_views);
  std::swap(m_allocator, rhs.m_allocator);
  std::swap(m_allocator_stage, rhs.m_allocator_stage);
  std::swap(m_buffer, rhs.m_buffer);
  std::swap(m_buffer_stage, rhs.m_buffer_stage);
  std::swap(m_ptr_mem_stage, rhs.m_ptr_mem_stage);
}

glm::fmat4 const& TransformDatabase::get(std::string const& name) {
  return *reinterpret_cast<glm::fmat4 const*>(m_ptr_mem_stage + m_views.at(index(name)).offset());
}

void TransformDatabase::set(std::string const& name, glm::fmat4 const& mat) {
  std::memcpy(m_ptr_mem_stage + m_views.at(index(name)).offset(), &mat, sizeof(mat));
}