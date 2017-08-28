#include "wrap/descriptor_pool_info.hpp"
#include "texture_loader.hpp"
#include "geometry_loader.hpp"
#include "wrap/conversions.hpp"

// c++ warpper
#include <vulkan/vulkan.hpp>

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <mpi.h>

#include <iostream>
#include <chrono>

template<typename T>
cmdline::parser ApplicationPresent<T>::getParser() {
  cmdline::parser cmd_parse{T::getParser()};
  cmd_parse.add<int>("cut", 'c', "cut size in MB, 0 - fourth of leaf level size", false, 0, cmdline::range(0, 1024 * 64));
  cmd_parse.add<int>("upload", 'u', "upload size in MB, 0 - 1/16 of leaf size", false, 0, cmdline::range(0, 1500));
  return cmd_parse;
}

template<typename T>
ApplicationPresent<T>::ApplicationPresent(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :T{resource_path, device, surf, cmd_parse}
 ,m_ptr_buff_transfer{nullptr}
 ,m_frustum_cells{0}
{  
  if (MPI::COMM_WORLD.Get_size() < 2) {
    throw std::runtime_error{"Error - only one thread!"};
  }

  this->m_statistics.addTimer("receive");

  createFrustra();
  glm::uvec2 cell_resolution{this->m_resolution / m_frustum_cells};
  MPI::COMM_WORLD.Bcast((void*)&cell_resolution, 2, MPI::UNSIGNED, 0);

  createReceiveBuffer();

  this->createRenderResources();
}

template<typename T>
ApplicationPresent<T>::~ApplicationPresent<T>() {
  this->m_buffers.at("transfer").unmap();

  this->shutDown();

  std::cout << "Presenter" << std::endl;
  std::cout << "Receive time: " << this->m_statistics.get("receive") << " milliseconds " << std::endl;
}

template<typename T>
FrameResource ApplicationPresent<T>::createFrameResource() {
  auto res = T::createFrameResource();
  return res;
}

template<typename T>
void ApplicationPresent<T>::updateFrameResources() {
  for (auto& res : this->m_frame_resources) {
    this->updateResourceDescriptors(res);
    this->updateResourceCommandBuffers(res);
    auto extent = extent_3d(this->m_resolution); 
    res.buffer_views["transfer"] = BufferView{extent.width * extent.height * sizeof(glm::u8vec4), vk::BufferUsageFlagBits::eTransferSrc};
    res.buffer_views.at("transfer").bindTo(this->m_buffers.at("transfer"));
  }
}

template<typename T>
void ApplicationPresent<T>::createFrustra() {
  this->m_frustra.clear();
  glm::fmat4 const& projection = this->m_camera.projectionMatrix();

  // general case
  Frustum2 frustum{};
  frustum.update(projection);
  // base point
  glm::fvec3 const& base = frustum.points[6];
  glm::fvec3 const& back = frustum.points[7];
  // edges
  glm::fvec3 right = frustum.points[4] - base;
  glm::fvec3 up = frustum.points[2] - base;
  auto test = glm::frustum(base.x, base.x + right.x, base.y, base.y + up.y, -base.z, -back.z);
  // std::cout << base.z << " far " << back.z << std::endl;

  unsigned num_workers = MPI::COMM_WORLD.Get_size() - 1;
  m_frustum_cells = glm::uvec2{1, 1};
  if (num_workers == 1) {
    this->m_frustra.emplace_back(test);
  }
  else {
    float l2 = float(log2(num_workers));

    glm::fvec2 frustum_dims{right.x, up.y};
    glm::fvec2 cell_dims{right.x, up.y};
    while(l2 > 0) {
      // split larger dimension
      if (cell_dims.x > cell_dims.y) {
        m_frustum_cells.x *= 2;
      }
      else {
        m_frustum_cells.y *= 2;
      }
      cell_dims = frustum_dims / glm::fvec2{m_frustum_cells};
      l2 -= 1.0f;
    }
    std::cout << "Cell resolution: " << m_frustum_cells.x << " x " << m_frustum_cells.y << std::endl;
    for (unsigned i = 0; i < num_workers; ++i) {
      glm::uvec2 cell_coord{i % m_frustum_cells.x, i / m_frustum_cells.x};

      glm::fvec2 base_coord = glm::fvec2{base} + glm::fvec2{cell_coord} * cell_dims; 
      this->m_frustra.emplace_back(glm::frustum(base_coord.x, base_coord.x + cell_dims.x, base_coord.y, base_coord.y + cell_dims.y, -base.z, -back.z));
    }
  }
  // send resolution to workers
  glm::uvec2 cell_resolution{this->m_resolution / m_frustum_cells};
  // generate copy regions for runtime
  vk::ImageSubresourceLayers subresource;
  subresource.aspectMask = vk::ImageAspectFlagBits::eColor;
  subresource.mipLevel = 0; 
  subresource.baseArrayLayer = 0; 
  subresource.layerCount = 1; 

  this->m_copy_regions = std::vector<std::vector<vk::BufferImageCopy>>{this->m_frame_resources.size(), std::vector<vk::BufferImageCopy>{}};
  int size_chunk = int(cell_resolution.x * cell_resolution.y * 4);
  int size_image = int(this->m_resolution.x * this->m_resolution.y * 4);
  // for each frame resource
  for (unsigned i = 0; i < this->m_frame_resources.size(); ++i) {
    // for each worker
    for (unsigned j = 0; j < num_workers; ++j) {
      vk::BufferImageCopy region{};
      region.bufferOffset = size_image * i + size_chunk * j;
      region.bufferRowLength = 0;
      region.bufferImageHeight = 0;
      region.imageSubresource = subresource;
      region.imageOffset = vk::Offset3D{int32_t(j % m_frustum_cells.x * cell_resolution.x), int32_t(j / m_frustum_cells.x * cell_resolution.x), 0};
      region.imageExtent = vk::Extent3D{cell_resolution.x, cell_resolution.y, 1};
      this->m_copy_regions[i].emplace_back(region);
    }
  }
}

template<typename T>
void ApplicationPresent<T>::receiveData(FrameResource& res) {
  this->m_statistics.start("receive");
  glm::uvec2 res_worker = this->m_resolution / m_frustum_cells;
  int size_chunk = int(res_worker.x * res_worker.y * 4);
  // int size_image = int(this->m_resolution.x * this->m_resolution.y * 4);
  // copy into current subregion
  size_t offset = res.buffer_views.at("transfer").offset();
  // copy chunk from process [1] to beginning
  offset -= size_chunk; 
  MPI::COMM_WORLD.Gather(MPI::IN_PLACE, size_chunk, MPI::BYTE, m_ptr_buff_transfer + offset, size_chunk, MPI::BYTE, 0);
  this->m_statistics.stop("receive");
}

template<typename T>
void ApplicationPresent<T>::recordDrawBuffer(FrameResource& res) {
  receiveData(res);

  res.command_buffers.at("draw")->reset({});

  res.command_buffers.at("draw")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  res.command_buffers.at("draw").transitionLayout(*res.target_view, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

  res.command_buffers.at("draw").copyBufferToImage(this->m_buffers.at("transfer"), *res.target_view, vk::ImageLayout::eTransferDstOptimal, m_copy_regions[res.buffer_views.at("transfer").offset() / res.buffer_views.at("transfer").size()]);

  res.command_buffers.at("draw").transitionLayout(*res.target_view, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

  res.command_buffers.at("draw")->end();
}

template<typename T>
void ApplicationPresent<T>::createReceiveBuffer() {
  auto extent = extent_3d(this->m_resolution); 

  this->m_buffers["transfer"] = Buffer{this->m_device, extent.width * extent.height * sizeof(glm::u8vec4) * this->m_frame_resources.size(), vk::BufferUsageFlagBits::eTransferSrc};

  this->m_memory_image = Memory{this->m_device, this->m_buffers.at("transfer").memoryTypeBits(), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, this->m_buffers.at("transfer").size()};
  this->m_buffers.at("transfer").bindTo(this->m_memory_image, 0);

  m_ptr_buff_transfer = (uint8_t*)this->m_buffers.at("transfer").map();
}

template<typename T>
void ApplicationPresent<T>::logic() {
  // resolution
  glm::uvec2 res_worker = this->m_resolution / m_frustum_cells;
  MPI::COMM_WORLD.Bcast((void*)&res_worker, 2, MPI::UNSIGNED, 0);
  // update camera
  T::logic();
  // broadcast matrices
  // identical view
  MPI::COMM_WORLD.Bcast((void*)&this->m_camera.viewMatrix(), 16, MPI::FLOAT, 0);
  //different projection for each, copy frustra[0] to process 1 
  MPI::COMM_WORLD.Scatter(this->m_frustra.data() - 1, 16, MPI::FLOAT, MPI::IN_PLACE, 16, MPI::FLOAT, 0);
}

template<typename T>
void ApplicationPresent<T>::onResize() {
  createFrustra();
  createReceiveBuffer();
}

// broadcast if shutdown
template<typename T>
void ApplicationPresent<T>::onFrameEnd() {
  uint8_t flag = this->shouldClose() ? 1 : 0;
  MPI::COMM_WORLD.Bcast(&flag, 1, MPI_BYTE, 0);
}
