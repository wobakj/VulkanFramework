#include "application_present.hpp"

#include "app/launcher_win.hpp"
#include "wrap/descriptor_pool_info.hpp"
#include "texture_loader.hpp"
#include "geometry_loader.hpp"

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

cmdline::parser ApplicationPresent::getParser() {
  cmdline::parser cmd_parse{ApplicationSingle::getParser()};
  cmd_parse.add<int>("cut", 'c', "cut size in MB, 0 - fourth of leaf level size", false, 0, cmdline::range(0, 1024 * 64));
  cmd_parse.add<int>("upload", 'u', "upload size in MB, 0 - 1/16 of leaf size", false, 0, cmdline::range(0, 1500));
  return cmd_parse;
}

ApplicationPresent::ApplicationPresent(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :ApplicationSingle{resource_path, device, surf, cmd_parse}
 ,m_ptr_buff_transfer{nullptr}
 ,m_frustum_cells{0}
{  

  if (MPI::COMM_WORLD.Get_size() < 1) {
    std::cerr << "Error - only one thread!" << std::endl;
  }

  m_shaders.emplace("compute", Shader{m_device, {m_resource_path + "shaders/pattern_comp.spv"}});

  createFrustra();

  createUniformBuffers();

  createRenderResources();
}

ApplicationPresent::~ApplicationPresent() {
  m_buffers.at("transfer").unmap();

  shutDown();
}

FrameResource ApplicationPresent::createFrameResource() {
  auto res = ApplicationSingle::createFrameResource();
  res.command_buffers.emplace("transfer", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  return res;
}

void ApplicationPresent::createFrustra() {
  m_frustra.clear();
  glm::fmat4 const& projection = m_camera.projectionMatrix();
  // special case

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
  std::cout << base.z << " far " << back.z << std::endl;
  // assert(test == projection);
  unsigned num_workers = MPI::COMM_WORLD.Get_size() - 1;
  m_frustum_cells = glm::uvec2{1, 1};
  if (num_workers == 1) {
    m_frustra.emplace_back(test);
  }
  else {
    float l2 = float(log2(num_workers));
    assert(float(int(l2)) == l2);

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
      // m_frustra.emplace_back(projection);
      glm::fvec2 base_coord = glm::fvec2{base} + glm::fvec2{cell_coord} * cell_dims; 
      m_frustra.emplace_back(glm::frustum(base_coord.x, base_coord.x + cell_dims.x, base_coord.y, base_coord.y + cell_dims.y, -base.z, -back.z));
    }
  }
  // send resolution to workers
  glm::uvec2 cell_resolution{m_resolution / m_frustum_cells};
  MPI::COMM_WORLD.Bcast((void*)&cell_resolution, 2, MPI::UNSIGNED, 0);

  vk::ImageSubresourceLayers subresource;
  subresource.aspectMask = vk::ImageAspectFlagBits::eColor;
  subresource.mipLevel = 0; 
  subresource.baseArrayLayer = 0; 
  subresource.layerCount = 1; 

  m_copy_regions.clear();
  int size_chunk = int(cell_resolution.x * cell_resolution.y * 4);
  for (unsigned i = 0; i < num_workers; ++i) {
    vk::BufferImageCopy region{};
    region.bufferOffset = size_chunk * i;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = subresource;
    region.imageOffset = vk::Offset3D{int32_t(i % m_frustum_cells.x * cell_resolution.x), int32_t(i / m_frustum_cells.x * cell_resolution.x), 0};
    region.imageExtent = vk::Extent3D{cell_resolution.x, cell_resolution.y, 1};
    m_copy_regions.emplace_back(region);
  }
}

void ApplicationPresent::receiveData() {
  glm::uvec2 res_worker = m_resolution / m_frustum_cells;
  int size_chunk = int(res_worker.x * res_worker.y * 4);
  // copy chunk from process [1] to behinning
  MPI::COMM_WORLD.Gather(MPI::IN_PLACE, size_chunk, MPI::BYTE, m_ptr_buff_transfer - size_chunk, size_chunk, MPI::BYTE, 0);
}

void ApplicationPresent::recordDrawBuffer(FrameResource& res) {
  receiveData();

  res.command_buffers.at("draw")->reset({});

  res.command_buffers.at("draw")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  res.command_buffers.at("draw").transitionLayout(*res.target_view, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

  res.command_buffers.at("draw").copyBufferToImage(m_buffers.at("transfer"), *res.target_view, vk::ImageLayout::eTransferDstOptimal, m_copy_regions);

  res.command_buffers.at("draw").transitionLayout(*res.target_view, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

  res.command_buffers.at("draw")->end();
}

void ApplicationPresent::createUniformBuffers() {
  auto extent = extent_3d(m_swap_chain.extent()); 
  // create upload buffer
  std::vector<glm::u8vec4> color_data{extent.width * extent.height, glm::u8vec4{255, 0, 0, 255}};
  for(unsigned x = 0; x < extent.width; ++x) {
    for(unsigned y = 0; y < extent.height; ++y) {
      color_data[y * extent.width + x] = glm::u8vec4{uint8_t(float(MPI::COMM_WORLD.Get_rank()) / float(MPI::COMM_WORLD.Get_size()) * 255), uint8_t(float(y) / float(extent.height) * 255), uint8_t(float(x) / float(extent.width) * 255), 255};
    }
  }
  m_buffers["transfer"] = Buffer{m_device, color_data.size() * sizeof(color_data.front()), vk::BufferUsageFlagBits::eTransferSrc};
  // m_buffers["transfer"] = Buffer{m_device, m_images.at("texture").size(), vk::BufferUsageFlagBits::eTransferSrc};
  m_memory_image = Memory{m_device, m_buffers.at("transfer").memoryTypeBits(), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, m_buffers.at("transfer").size()};
  m_buffers.at("transfer").bindTo(m_memory_image, 0);
  m_buffers.at("transfer").setData(color_data.data(), color_data.size() * sizeof(color_data.front()), 0);
  m_ptr_buff_transfer = (uint8_t*)m_buffers.at("transfer").map();

  m_buffers["time"] = Buffer{m_device, sizeof(float), vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};
  m_allocators.at("buffers").allocate(m_buffers.at("time"));
}

void ApplicationPresent::logic() {
  // update camera
  ApplicationSingle::logic();
  // broadcast matrices
  // identical view
  MPI::COMM_WORLD.Bcast((void*)&m_camera.viewMatrix(), 16, MPI::FLOAT, 0);
  //different projection for each, copy frustra[0] to process 1 
  MPI::COMM_WORLD.Scatter(m_frustra.data() - 1, 16, MPI::FLOAT, MPI::IN_PLACE, 16, MPI::FLOAT, 0);
}

void ApplicationPresent::onResize(std::size_t width, std::size_t height) {
  createFrustra();
}

// broadcast if shutdown
void ApplicationPresent::onFrameEnd() {
  uint8_t flag = shouldClose() ? 1 : 0;
  MPI::COMM_WORLD.Bcast(&flag, 1, MPI_BYTE, 0);
}
///////////////////////////// misc functions ////////////////////////////////

// // exe entry point
// int main(int argc, char* argv[]) {
//   LauncherWin::run<ApplicationPresent>(argc, argv);
// }