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
  m_buffer = Buffer{*m_device, sizeof(glm::fmat4) * 100, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst};
  auto mem_type = m_device->findMemoryType(m_buffer.requirements().memoryTypeBits 
                                           , vk::MemoryPropertyFlagBits::eDeviceLocal);
  m_allocator = StaticAllocator(*m_device, mem_type, m_buffer.requirements().size);
  m_allocator.allocate(m_buffer);

  m_buffer_stage = Buffer{*m_device, sizeof(glm::fmat4) * 100, vk::BufferUsageFlagBits::eTransferSrc};
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
  m_indices.emplace(name, m_indices.size());
  // storge gpu representation
  set(name, resource);
}

size_t TransformDatabase::index(std::string const& name) const {
  return m_indices.at(name);
}

void TransformDatabase::swap(TransformDatabase& rhs) {
  Database::swap(rhs);
  std::swap(m_indices, rhs.m_indices);
  // std::swap(m_views, rhs.m_views);
  std::swap(m_allocator, rhs.m_allocator);
  std::swap(m_allocator_stage, rhs.m_allocator_stage);
  std::swap(m_buffer, rhs.m_buffer);
  std::swap(m_buffer_stage, rhs.m_buffer_stage);
  std::swap(m_ptr_mem_stage, rhs.m_ptr_mem_stage);
  std::swap(m_dirties, rhs.m_dirties);
}

glm::fmat4 const& TransformDatabase::get(std::string const& name) {
  return *reinterpret_cast<glm::fmat4 const*>(m_ptr_mem_stage + index(name) * SIZE_RESOURCE);
}

void TransformDatabase::set(std::string const& name, glm::fmat4 const& mat) {
  auto const& index_transform = index(name);
  m_dirties.emplace_back(index_transform);
  std::memcpy(m_ptr_mem_stage + SIZE_RESOURCE * index_transform, &mat, SIZE_RESOURCE);
}

void TransformDatabase::updateCommand(CommandBuffer& command_buffer) const {
  if (m_dirties.empty()) return;

  std::cout << "updating " << m_dirties.size() << " transforms" << std::endl;
  std::vector<vk::BufferCopy> copy_views{};
  for(auto const& dirty_index : m_dirties) {
    auto const& offset = dirty_index * SIZE_RESOURCE;
    copy_views.emplace_back(offset, offset, SIZE_RESOURCE);
  }
  command_buffer->copyBuffer(m_buffer_stage, m_buffer, copy_views);
  // barrier to make new data visible to vertex shader
  vk::BufferMemoryBarrier barrier{};
  barrier.buffer = m_buffer;
  barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
  barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  command_buffer->pipelineBarrier(
    vk::PipelineStageFlagBits::eTransfer,
    vk::PipelineStageFlagBits::eVertexShader,
    vk::DependencyFlags{},
    {},
    {barrier},
    {}
  );

  m_dirties.clear();
}