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
 ,m_sema_image_ready{m_device, vkDestroySemaphore}
 // ,m_shaders{}
{
	initialize();
}

Application::~Application() {
	 // free resources
  for(auto const& command_buffer : m_command_buffers) {
    m_device->freeCommandBuffers(m_device.pool("graphics"), {command_buffer.second});    
  }
}

void Application::initialize() {
  m_sema_image_ready = m_device->createSemaphore({});
}

void Application::updateShaderPrograms() {
	for (auto& pair : m_shaders) {
    m_shaders.at("first") =  Shader{m_device, pair.second.m_paths};
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


uint32_t Application::acquireImage() {
  uint32_t imageIndex;
  auto result = m_device->acquireNextImageKHR(m_swap_chain, std::numeric_limits<uint64_t>::max(), m_sema_image_ready.get(), VK_NULL_HANDLE, &imageIndex);
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
