#include "application.hpp"

#include "shader.hpp"
#include "image.hpp"
#include "buffer.hpp"

#include <iostream>

Application::Application(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window)
 :m_resource_path{resource_path}
 ,m_camera{45.0f, 10, 10, 0.1f, 500.0f, window}
 ,m_device(device)
 ,m_swap_chain(chain)
{
	initialize();
}

Application::~Application() {
	 // free resources
  for(auto const& command_buffer : m_command_buffers) {
    m_device->freeCommandBuffers(m_device.pool("graphics"), {command_buffer.second});    
  }
  for(auto const& semaphore : m_semaphores) {
    m_device->destroySemaphore(semaphore.second);    
  }
  for(auto const& fence : m_fences) {
    m_device->destroyFence(fence.second);    
  }
}

void Application::initialize() {
  m_semaphores.emplace("acquire", m_device->createSemaphore({}));
  m_semaphores.emplace("draw", m_device->createSemaphore({}));
  m_fences.emplace("draw", m_device->createFence({}));
  m_fences.emplace("acquire", m_device->createFence({}));
  m_device->resetFences({fenceDraw()});

}

void Application::updateShaderPrograms() {
	for (auto& pair : m_shaders) {
    pair.second = Shader{m_device, pair.second.m_paths};
  }
  recreatePipeline();
}

void Application::resize(std::size_t width, std::size_t height) {
  m_camera.setAspect(width, height);
  resize();
}

void Application::update() {
  static double time_last = glfwGetTime();
	// calculate delta time
	double time_current = glfwGetTime();
	float time_delta = float(time_current - time_last);
	time_last = time_current;
	// update buffers
	m_camera.update(time_delta);
	if (m_camera.changed()) {
	  updateView();
	}
	// do actual rendering
	render();
}

vk::Semaphore const& Application::semaphoreAcquire() {
  return m_semaphores.at("acquire");
}

vk::Semaphore const& Application::semaphoreDraw() {
  return m_semaphores.at("draw");
}

vk::Fence const& Application::fenceDraw() {
  return m_fences.at("draw");
}

vk::Fence const& Application::fenceAcquire() {
  return m_fences.at("acquire");
}

uint32_t Application::acquireImage() {
  uint32_t imageIndex;
  auto result = m_device->acquireNextImageKHR(m_swap_chain, std::numeric_limits<uint64_t>::max(), semaphoreAcquire(), fenceAcquire(), &imageIndex);
  if (result == vk::Result::eErrorOutOfDateKHR) {
  		// handle swapchain recreation
      // recreateSwapChain();
      imageIndex = std::numeric_limits<uint32_t>::max();
      throw std::runtime_error("swapchain out if date!");
  } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
      throw std::runtime_error("failed to acquire swap chain image!");
  }
  return imageIndex;
}

void Application::present(uint32_t index_image) {
  vk::PresentInfoKHR presentInfo{};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &semaphoreDraw();

  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &m_swap_chain.get();
  presentInfo.pImageIndices = &index_image;

  m_device.getQueue("present").presentKHR(presentInfo);
  m_device.getQueue("present").waitIdle();
}

void Application::submitDraw(vk::CommandBuffer const& buffer) {
  std::vector<vk::SubmitInfo> submitInfos(1,vk::SubmitInfo{});

  vk::Semaphore waitSemaphores[]{semaphoreAcquire()};
  vk::PipelineStageFlags waitStages[]{vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submitInfos[0].setWaitSemaphoreCount(1);
  submitInfos[0].setPWaitSemaphores(waitSemaphores);
  submitInfos[0].setPWaitDstStageMask(waitStages);

  submitInfos[0].setCommandBufferCount(1);
  submitInfos[0].setPCommandBuffers(&buffer);

  vk::Semaphore signalSemaphores[]{semaphoreDraw()};
  submitInfos[0].signalSemaphoreCount = 1;
  submitInfos[0].pSignalSemaphores = signalSemaphores;

  m_device->resetFences({fenceDraw()});
  m_device.getQueue("graphics").submit(submitInfos, fenceDraw());
}

void Application::blockSwapChain() {
  m_mutex_swapchain.lock();
}
void Application::unblockSwapChain() {
  m_mutex_swapchain.unlock();
}