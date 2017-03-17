#include "ren/database_camera.hpp"

#include "ren/gpu_camera.hpp"

#include "wrap/device.hpp"
#include "wrap/image.hpp"
#include "texture_loader.hpp"
#include "transferrer.hpp"

#include <utility>


CameraDatabase::CameraDatabase()
 :Database{}
{}

CameraDatabase::CameraDatabase(CameraDatabase && rhs)
{
  swap(rhs);
}

CameraDatabase::CameraDatabase(Device const& device)
 :Database{}
{
  m_device = & device;
  m_buffer = Buffer{*m_device, sizeof(gpu_camera_t) * 100, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer};
  auto mem_type = m_device->findMemoryType(m_buffer.requirements().memoryTypeBits 
                                           , vk::MemoryPropertyFlagBits::eDeviceLocal);
  m_allocator = StaticAllocator(*m_device, mem_type, m_buffer.requirements().size);
  m_allocator.allocate(m_buffer);

  m_buffer_stage = Buffer{*m_device, sizeof(gpu_camera_t) * 100, vk::BufferUsageFlagBits::eTransferSrc};
  mem_type = m_device->findMemoryType(m_buffer_stage.requirements().memoryTypeBits 
                                           , vk::MemoryPropertyFlagBits::eHostVisible
                                           | vk::MemoryPropertyFlagBits::eHostCoherent);
  m_allocator_stage = StaticAllocator(*m_device, mem_type, m_buffer_stage.requirements().size);
  m_allocator_stage.allocate(m_buffer_stage);

  m_ptr_mem_stage = static_cast<uint8_t*>(m_buffer_stage.map());
}

CameraDatabase& CameraDatabase::operator=(CameraDatabase&& rhs) {
  swap(rhs);
  return *this;
}

void CameraDatabase::store(std::string const& name, Camera&& resource) {
  m_indices.emplace(name, m_indices.size());
  // storge gpu representation
  Database::store(name, std::move(resource));
}

size_t CameraDatabase::index(std::string const& name) const {
  return m_indices.at(name);
}

void CameraDatabase::swap(CameraDatabase& rhs) {
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

// Camera const& CameraDatabase::get(std::string const& name) {
//   return *reinterpret_cast<Camera const*>(m_ptr_mem_stage + index(name) * sizeof(gpu_camera_t));
// }

void CameraDatabase::set(std::string const& name, Camera&& cam) {
  // set gpu camera
  gpu_camera_t gpu_cam{cam.viewMatrix(), cam.projectionMatrix()};
  auto const& index_transform = index(name);
  m_dirties.emplace_back(index_transform);
  std::memcpy(m_ptr_mem_stage + sizeof(gpu_camera_t) * index_transform, &gpu_cam, sizeof(gpu_camera_t));
  // set cpu camera
  Database::set(name, std::move(cam));
}

void CameraDatabase::set(std::string const& name, Camera const& cam) {
  set(name, Camera{cam});
}

void CameraDatabase::updateCommand(CommandBuffer const& command_buffer) const {
  if (m_dirties.empty()) return;

  std::vector<vk::BufferCopy> copy_views{};
  for(auto const& dirty_index : m_dirties) {
    auto const& offset = dirty_index * sizeof(gpu_camera_t);
    copy_views.emplace_back(offset, offset, sizeof(gpu_camera_t));
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