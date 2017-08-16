#include "app/application_win.hpp"

#include "wrap/surface.hpp"
#include "wrap/submit_info.hpp"

#include "frame_resource.hpp"
//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>

cmdline::parser ApplicationWin::getParser() {
  cmdline::parser cmd_parse{Application::getParser()};
  cmd_parse.add("present", 'p', "present mode", false, std::string{"fifo"}, cmdline::oneof<std::string>("fifo", "mailbox", "immediate"));
  return cmd_parse;
}

ApplicationWin::ApplicationWin(std::string const& resource_path, Device& device, Surface const& surf, uint32_t image_count, cmdline::parser const& cmd_parse)
 :Application{resource_path, device, cmd_parse}
 ,m_camera{45.0f, 1.0f, 0.1f, 500.0f, &surf.window()}
 ,m_surface{&surf}
{
  int width, height = -1;
  glfwGetWindowSize(&surf.window(), &width, &height);
  m_resolution = glm::uvec2{width, height};

  createSwapChain(surf, cmd_parse, image_count);

  m_statistics.addTimer("fence_acquire");
  m_statistics.addTimer("queue_present");
}

ApplicationWin::~ApplicationWin() {
  std::cout << std::endl;
  std::cout << "Average acquire fence time: " << m_statistics.get("fence_acquire") << " milliseconds" << std::endl;
  std::cout << "Average present queue time: " << m_statistics.get("queue_present") << " milliseconds " << std::endl;
}

void ApplicationWin::createSwapChain(Surface const& surf, cmdline::parser const& cmd_parse, uint32_t image_count) {
  vk::PresentModeKHR present_mode{};
  std::string mode = cmd_parse.get<std::string>("present");
  if (mode == "fifo") {
    present_mode = vk::PresentModeKHR::eFifo;
  }
  else if (mode == "mailbox") {
    present_mode = vk::PresentModeKHR::eMailbox;
  }
  else if (mode == "immediate") {
    present_mode = vk::PresentModeKHR::eImmediate;
  }
  m_swap_chain.create(m_device, surf, vk::Extent2D{m_resolution.x, m_resolution.y}, present_mode, image_count);
}

FrameResource ApplicationWin::createFrameResource() {
  auto res = Application::createFrameResource();
  res.addSemaphore("acquire");
  res.addFence("acquire");
  // present must wait for draw
  res.addSemaphore("draw");
  return res;
}

void ApplicationWin::acquireImage(FrameResource& res) {
  // wait for last acquisition until acquiring again
  m_statistics.start("fence_acquire");
  res.fence("acquire").wait();
  m_statistics.stop("fence_acquire");

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
  res.target_view = &m_swap_chain.view(res.image);
}

void ApplicationWin::presentFrame(FrameResource& res) {
  presentFrame(res, m_device.getQueue("present"));
}

void ApplicationWin::presentFrame(FrameResource& res, vk::Queue const& queue) {
  m_statistics.start("queue_present");
  
  vk::PresentInfoKHR presentInfo{};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &res.semaphore("draw");

  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &m_swap_chain.get();
  presentInfo.pImageIndices = &res.image;

  // do wait before present to prevent cpu stalling
  m_device.getQueue("present").waitIdle();
  queue.presentKHR(presentInfo);
  m_statistics.stop("queue_present");
}

void ApplicationWin::resize(std::size_t width, std::size_t height) {
  m_swap_chain.recreate(vk::Extent2D{uint32_t(width), uint32_t(height)});
  m_camera.setAspect(float(width) / float(height));

  Application::resize(width, height);
}

void ApplicationWin::onFrameBegin() {
  static double time_last = glfwGetTime();
  // calculate delta time
  double time_current = glfwGetTime();
  float time_delta = float(time_current - time_last);
  time_last = time_current;
  m_camera.update(time_delta);
}

glm::fmat4 const& ApplicationWin::matrixView() const {
  return m_camera.viewMatrix();
}

glm::fmat4 const& ApplicationWin::matrixFrustum() const {
  return m_camera.projectionMatrix();
}

bool ApplicationWin::shouldClose() const{
  return glfwWindowShouldClose(&m_surface->window());
}

SubmitInfo ApplicationWin::createDrawSubmitInfo(FrameResource const& res) const {
  SubmitInfo info = Application::createDrawSubmitInfo(res);
  info.addWaitSemaphore(res.semaphore("acquire"), vk::PipelineStageFlagBits::eColorAttachmentOutput);
  info.addSignalSemaphore(res.semaphore("draw"));
  return info;
}

void ApplicationWin::keyCallbackSelf(int key, int scancode, int action, int mods) {
  if ((key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(&m_surface->window(), 1);
  }
  // call child function
  keyCallback(key, scancode, action, mods);
}
