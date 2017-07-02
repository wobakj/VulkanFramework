#include "app/application.hpp"
#include "app/launcher.hpp"

#include "frame_resource.hpp"
//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>

cmdline::parser Application::getParser() {
  cmdline::parser cmd_parse{};
  return cmd_parse;
}

Application::Application(std::string const& resource_path, Device& device, cmdline::parser const& cmd_parse)
 :m_resource_path{resource_path}
 ,m_device(device)
 ,m_pipeline_cache{m_device}
 ,m_resolution{0, 0}
{
  createMemoryPools();
  createCommandPools();

  m_transferrer = Transferrer{m_command_pools.at("transfer")};
}

FrameResource Application::createFrameResource() {
  auto res = FrameResource{m_device};
  res.addSemaphore("draw");
  res.addFence("draw");
  res.addCommandBuffer("draw", std::move(m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::ePrimary)));
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

void Application::submitDraw(FrameResource& res) {
  std::vector<vk::SubmitInfo> submitInfos(1,vk::SubmitInfo{});

  vk::Semaphore waitSemaphores[]{res.semaphore("acquire")};
  vk::PipelineStageFlags waitStages[]{vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submitInfos[0].setWaitSemaphoreCount(1);
  submitInfos[0].setPWaitSemaphores(waitSemaphores);
  submitInfos[0].setPWaitDstStageMask(waitStages);

  submitInfos[0].setCommandBufferCount(1);
  submitInfos[0].setPCommandBuffers(&res.command_buffers.at("draw").get());

  vk::Semaphore signalSemaphores[]{res.semaphore("draw")};
  submitInfos[0].signalSemaphoreCount = 1;
  submitInfos[0].pSignalSemaphores = signalSemaphores;

  res.fence("draw").reset();
  m_device.getQueue("graphics").submit(submitInfos, res.fence("draw"));
}

void Application::updateShaderPrograms() {
	for (auto& pair : m_shaders) {
    pair.second = Shader{m_device, pair.second.paths()};
  }
  recreatePipeline();
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