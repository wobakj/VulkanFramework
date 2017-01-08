#include "application.hpp"

#include "frame_resource.hpp"

#include <iostream>

Application::Application(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window)
 :m_resource_path{resource_path}
 ,m_camera{45.0f, 10, 10, 0.1f, 500.0f, window}
 ,m_device(device)
 ,m_swap_chain(chain)
{}

void Application::updateShaderPrograms() {
	for (auto& pair : m_shaders) {
    pair.second = Shader{m_device, pair.second.paths()};
  }
  recreatePipeline();
}

void Application::resize(std::size_t width, std::size_t height) {
  m_camera.setAspect(width, height);
  resize();
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
  update();
	// do actual rendering
	render();
}

void Application::acquireImage(FrameResource& res) {
  res.fenceAcquire().reset();
  auto result = m_device->acquireNextImageKHR(m_swap_chain, std::numeric_limits<uint64_t>::max(), res.semaphoreAcquire(), res.fenceAcquire(), &res.image);
  if (result == vk::Result::eErrorOutOfDateKHR) {
      // handle swapchain recreation
      // recreateSwapChain();
      throw std::runtime_error("swapchain out if date!");
  } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
      throw std::runtime_error("failed to acquire swap chain image!");
  }
}

void Application::present(FrameResource& res) {
  vk::PresentInfoKHR presentInfo{};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &res.semaphoreDraw();

  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &m_swap_chain.get();
  presentInfo.pImageIndices = &res.image;

  m_device.getQueue("present").presentKHR(presentInfo);
  m_device.getQueue("present").waitIdle();
}

void Application::submitDraw(FrameResource& res) {
  std::vector<vk::SubmitInfo> submitInfos(1,vk::SubmitInfo{});

  vk::Semaphore waitSemaphores[]{res.semaphoreAcquire()};
  vk::PipelineStageFlags waitStages[]{vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submitInfos[0].setWaitSemaphoreCount(1);
  submitInfos[0].setPWaitSemaphores(waitSemaphores);
  submitInfos[0].setPWaitDstStageMask(waitStages);

  submitInfos[0].setCommandBufferCount(1);
  submitInfos[0].setPCommandBuffers(&res.command_buffers.at("draw"));

  vk::Semaphore signalSemaphores[]{res.semaphoreDraw()};
  submitInfos[0].signalSemaphoreCount = 1;
  submitInfos[0].pSignalSemaphores = signalSemaphores;

  res.fenceDraw().reset();
  m_device.getQueue("graphics").submit(submitInfos, res.fenceDraw());
}
