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
  return ApplicationSingle::getParser();
}

ApplicationPresent::ApplicationPresent(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :ApplicationSingle{resource_path, device, surf, cmd_parse}
{  
  m_shaders.emplace("compute", Shader{m_device, {m_resource_path + "shaders/pattern_comp.spv"}});

  createTextureImages();
  createUniformBuffers();

  createRenderResources();

}

ApplicationPresent::~ApplicationPresent() {
  shutDown();
}

FrameResource ApplicationPresent::createFrameResource() {
  auto res = ApplicationSingle::createFrameResource();
  res.command_buffers.emplace("transfer", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  return res;
}

void ApplicationPresent::updateResourceCommandBuffers(FrameResource& res) {
  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  res.command_buffers.at("transfer")->reset({});
  res.command_buffers.at("transfer")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});
  res.command_buffers.at("transfer")->bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline_compute);
  res.command_buffers.at("transfer")->bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_compute.layout(), 0, {m_descriptor_sets.at("storage")}, {});

  glm::uvec3 dims{m_images.at("texture").extent().width, m_images.at("texture").extent().height, m_images.at("texture").extent().depth};
  glm::uvec3 workers{16, 16, 1};
  // 512^2 threads in blocks of 16^2
  res.command_buffers.at("transfer")->dispatch(dims.x / workers.x, dims.y / workers.y, dims.z / workers.z); 

  res.command_buffers.at("transfer")->end();
}

void ApplicationPresent::recordDrawBuffer(FrameResource& res) {
  updateUniformBuffers();

  res.command_buffers.at("draw")->reset({});

  res.command_buffers.at("draw")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  res.command_buffers.at("draw").copyBufferToImage(m_buffers.at("image"), m_images.at("texture").view(), vk::ImageLayout::eTransferDstOptimal);

  res.command_buffers.at("draw").transitionLayout(m_images.at("texture"), vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal);

  res.command_buffers.at("draw").transitionLayout(*res.target_view, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
  res.command_buffers.at("draw").copyImage(m_images.at("texture").view(), vk::ImageLayout::eTransferSrcOptimal, *res.target_view, vk::ImageLayout::eTransferDstOptimal);
  res.command_buffers.at("draw").transitionLayout(*res.target_view, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);
  // transform back for next copy
  res.command_buffers.at("draw").transitionLayout(m_images.at("texture"), vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eTransferDstOptimal);
  res.command_buffers.at("draw")->end();
}

void ApplicationPresent::createPipelines() {
  ComputePipelineInfo info_pipe_comp;
  info_pipe_comp.setShader(m_shaders.at("compute"));
  m_pipeline_compute = ComputePipeline{m_device, info_pipe_comp, m_pipeline_cache};
}

void ApplicationPresent::updatePipelines() {
  auto info_pipe_comp = m_pipeline_compute.info();
  info_pipe_comp.setShader(m_shaders.at("compute"));
  m_pipeline_compute.recreate(info_pipe_comp);
}

void ApplicationPresent::createTextureImages() {
  auto extent = extent_3d(m_swap_chain.extent()); 
  m_images["texture"] = ImageRes{m_device, extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferSrc
                                                                                                   | vk::ImageUsageFlagBits::eTransferDst
                                                                                                   | vk::ImageUsageFlagBits::eStorage};
  m_allocators.at("images").allocate(m_images.at("texture"));
  m_transferrer.transitionToLayout(m_images.at("texture"), vk::ImageLayout::eTransferDstOptimal);
}

void ApplicationPresent::updateDescriptors() { 
  m_images.at("texture").view().writeToSet(m_descriptor_sets.at("storage"), 0, vk::DescriptorType::eStorageImage);
  
  m_buffers.at("time").writeToSet(m_descriptor_sets.at("storage"), 1, vk::DescriptorType::eUniformBuffer);
}

void ApplicationPresent::createDescriptorPools() {
  DescriptorPoolInfo info_pool{};
  info_pool.reserve(m_shaders.at("compute"), 0, 1);

  m_descriptor_pool = DescriptorPool{m_device, info_pool};
  m_descriptor_sets["storage"] = m_descriptor_pool.allocate(m_shaders.at("compute").setLayout(0));
}

void ApplicationPresent::createUniformBuffers() {
  auto extent = extent_3d(m_swap_chain.extent()); 
  // create upload buffer
  std::vector<glm::u8vec4> color_data{extent.width * extent.height, glm::u8vec4{255, 0, 0, 255}};
  m_buffers["image"] = Buffer{m_device, m_images.at("texture").size(), vk::BufferUsageFlagBits::eTransferSrc};
  m_memory_image = Memory{m_device, m_buffers.at("image").memoryTypeBits(), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, m_buffers.at("image").size()};
  m_buffers.at("image").bindTo(m_memory_image, 0);
  m_buffers.at("image").setData(color_data.data());

  m_buffers["time"] = Buffer{m_device, sizeof(float), vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};
  m_allocators.at("buffers").allocate(m_buffers.at("time"));
  
  // m_buffers["colors"] = Buffer{m_device, sizeof(glm::u8vec3) * extent.width * extent.height, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc};
  // m_allocators.at("buffers").allocate(m_buffers.at("image"));


  // m_transferrer.uploadBufferToImage(&color_data, m_buffers.at("image"));
}

void ApplicationPresent::updateUniformBuffers() {
  float time = float(glfwGetTime()) * 2.0f;
  m_transferrer.uploadBufferData(&time, m_buffers.at("time"));
}
///////////////////////////// misc functions ////////////////////////////////

// exe entry point
int main(int argc, char* argv[]) {
  LauncherWin::run<ApplicationPresent>(argc, argv);
}