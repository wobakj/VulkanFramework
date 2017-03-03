#include "app/application.hpp"

#include "frame_resource.hpp"

#include <iostream>

// child classes must overwrite
const uint32_t Application::imageCount = 0;

Application::Application(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window, cmdline::parser const& cmd_parse)
 :m_resource_path{resource_path}
 ,m_camera{45.0f, 10, 10, 0.1f, 500.0f, window}
 ,m_device(device)
 ,m_swap_chain(chain)
 ,m_pipeline_cache{m_device}
{
  createMemoryPools();
}

FrameResource Application::createFrameResource() {
  auto res = FrameResource{m_device};
  res.addSemaphore("acquire");
  res.addSemaphore("draw");
  res.addFence("draw");
  res.addFence("acquire");
  res.addCommandBuffer("draw", m_device.createCommandBuffer("graphics"));
  return res;
}

void Application::frame() {
  static double time_last = glfwGetTime();
  // calculate delta time
  double time_current = glfwGetTime();
  float time_delta = float(time_current - time_last);
  time_last = time_current;
  // update buffers
  m_camera.update(time_delta);
  // do logic
  logic();
  // do actual rendering
  render();
}

void Application::acquireImage(FrameResource& res) {
  res.fence("acquire").reset();
  auto result = m_device->acquireNextImageKHR(m_swap_chain, 1000, res.semaphore("acquire"), res.fence("acquire"), &res.image);
  if (result == vk::Result::eErrorOutOfDateKHR) {
      // handle swapchain recreation
      // recreateSwapChain();
      throw std::runtime_error("swapchain out if date!");
  } 
  else if (result == vk::Result::eTimeout) {
      throw std::runtime_error("image requiest time out!");
  }
  else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
      throw std::runtime_error("failed to acquire swap chain image!");
  }
}

void Application::presentFrame(FrameResource& res) {
  presentFrame(res, m_device.getQueue("graphics"));
}

void Application::presentFrame(FrameResource& res, vk::Queue const& queue) {
  vk::PresentInfoKHR presentInfo{};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &res.semaphore("draw");

  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &m_swap_chain.get();
  presentInfo.pImageIndices = &res.image;

  // do wait before present to prevent cpu stalling
  m_device.getQueue("present").waitIdle();
  queue.presentKHR(presentInfo);
}

void Application::submitDraw(FrameResource& res) {
  std::vector<vk::SubmitInfo> submitInfos(1,vk::SubmitInfo{});

  vk::Semaphore waitSemaphores[]{res.semaphore("acquire")};
  vk::PipelineStageFlags waitStages[]{vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submitInfos[0].setWaitSemaphoreCount(1);
  submitInfos[0].setPWaitSemaphores(waitSemaphores);
  submitInfos[0].setPWaitDstStageMask(waitStages);

  submitInfos[0].setCommandBufferCount(1);
  submitInfos[0].setPCommandBuffers(&res.command_buffers.at("draw"));

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
  m_camera.setAspect(width, height);
  // draw queue is emptied in launcher::resize
  createFramebufferAttachments();
  createFramebuffers();
  onResize(width, height);
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
  updateCommandBuffers();
}

void Application::createRenderResources() {
  createFrameResources();
  createFramebufferAttachments();
  createRenderPasses();
  createFramebuffers();
  createPipelines();
  updateDescriptors();
  updateCommandBuffers();
}

void Application::createMemoryPools() {
  // find memory type which supports optimal image and specific depth format
  auto type_img = m_device.suitableMemoryType(vk::Format::eD32Sfloat, vk::ImageTiling::eOptimal, vk::MemoryPropertyFlagBits::eDeviceLocal);
  m_allocators.emplace("images", BlockAllocator{m_device, type_img, 4 * 4 * 3840 * 2160});
  // separate allocator for buffers to not deal with buffer-image-granularity
  auto type_nuffer = m_device.suitableMemoryType(vk::BufferUsageFlagBits::eStorageBuffer
                                               | vk::BufferUsageFlagBits::eTransferDst 
                                               | vk::BufferUsageFlagBits::eTransferSrc 
                                               | vk::BufferUsageFlagBits::eUniformBuffer
                                               , vk::MemoryPropertyFlagBits::eDeviceLocal);
  m_allocators.emplace("buffers", BlockAllocator{m_device, type_nuffer, 4 * 16 * 128});
}
