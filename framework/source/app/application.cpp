#include "app/application.hpp"
#include "app/launcher.hpp"

#include "wrap/submit_info.hpp"
#include "frame_resource.hpp"
//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>

cmdline::parser Application::getParser() {
  cmdline::parser cmd_parse{};
  return cmd_parse;
}

Application::Application(std::string const& resource_path, Device& device, uint32_t num_frames, cmdline::parser const& cmd_parse)
 :m_resource_path{resource_path}
 ,m_device(device)
 ,m_pipeline_cache{m_device}
 ,m_resolution{0, 0}
{
  // cannot initialize in lst, otherwise deleted copy constructor is invoked
  m_frame_resources.resize(num_frames);
  createMemoryPools();
  createCommandPools();

  m_transferrer = Transferrer{m_command_pools.at("transfer")};
}

FrameResource Application::createFrameResource() {
  auto res = FrameResource{m_device};
  res.addFence("draw");
  res.setCommandBuffer("draw", std::move(m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::ePrimary)));
  return res;
}

void Application::frame() {
  // callback
  onFrameBegin();
  // do logic
  logic();
  // do actual rendering
  render();
  // callback
  onFrameEnd();
}

SubmitInfo Application::createDrawSubmitInfo(FrameResource const& res) const {
  SubmitInfo info{};
  info.addCommandBuffer(res.command_buffers.at("draw").get());
  return info;
}

void Application::submitDraw(FrameResource& res) {
  res.fence("draw").reset();
  m_device.getQueue("graphics").submit({createDrawSubmitInfo(res).get()}, res.fence("draw"));
}

// workaround to prevent having to store number of frames as extra member
void Application::createFrameResources() {
  // create resources for one less image than swap chain
  // only numImages - 1 images can be acquired at a time
  for (auto& res : this->m_frame_resources) {
    res = std::move(createFrameResource());
  }
}

void Application::updateShaderPrograms() {
	for (auto& pair : m_shaders) {
    pair.second = Shader{m_device, pair.second.paths()};
  }
  recreatePipeline();
}

void Application::updateCommandBuffers() {
  for (auto& res : m_frame_resources) {
    updateResourceCommandBuffers(res);
  }
}
void Application::updateResourcesDescriptors() {
  for (auto& res : m_frame_resources) {
    updateResourceDescriptors(res);
  }
}
void Application::resize(std::size_t width, std::size_t height) {
  m_resolution = glm::uvec2{width, height};
  // draw queue is emptied in launcher::resize
  createFramebufferAttachments();
  createFramebuffers();
  onResize(width, height);
  updateDescriptors();
  updateResourcesDescriptors();
  updateCommandBuffers();
}

void Application::onResize(std::size_t width, std::size_t height) {}

void Application::recreatePipeline() {
  // wait for avaiability of resources
  emptyDrawQueue();
  // update pipeline and descriptors
  updatePipelines();
  createDescriptorPools();
  updateDescriptors();
  updateResourcesDescriptors();
  updateCommandBuffers();
}

void Application::createRenderResources() {
  createFrameResources();
  createFramebufferAttachments();
  createRenderPasses();
  createFramebuffers();
  createPipelines();
  createDescriptorPools();
  updateDescriptors();
  updateResourcesDescriptors();
  updateCommandBuffers();
}

void Application::createMemoryPools() {
  // find memory type which supports optimal image and specific depth format
  auto type_img = m_device.suitableMemoryType(vk::Format::eD32Sfloat, vk::ImageTiling::eOptimal, vk::MemoryPropertyFlagBits::eDeviceLocal);
  m_allocators.emplace("images", BlockAllocator{m_device, type_img, 5 * 4 * 3840 * 2160});
  // separate allocator for buffers to not deal with buffer-image-granularity
  auto type_buffer = m_device.suitableMemoryType(vk::BufferUsageFlagBits::eStorageBuffer
                                               | vk::BufferUsageFlagBits::eTransferDst 
                                               | vk::BufferUsageFlagBits::eTransferSrc 
                                               | vk::BufferUsageFlagBits::eUniformBuffer
                                               , vk::MemoryPropertyFlagBits::eDeviceLocal);
  m_allocators.emplace("buffers", BlockAllocator{m_device, type_buffer, 4 * 16 * 128 * 2});
}

void Application::createCommandPools() {
  m_command_pools.emplace("graphics", CommandPool{m_device, m_device.getQueueIndex("graphics"), vk::CommandPoolCreateFlagBits::eResetCommandBuffer});
  m_command_pools.emplace("transfer", CommandPool{m_device, m_device.getQueueIndex("transfer"), vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer});
}