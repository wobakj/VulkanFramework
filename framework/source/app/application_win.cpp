#include "app/application_win.hpp"

#include "wrap/surface.hpp"

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
{
  createSwapChain(surf, cmd_parse, image_count);
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
  m_swap_chain.create(m_device, surf, vk::Extent2D{1280, 720}, present_mode, image_count);
}

FrameResource ApplicationWin::createFrameResource() {
  auto res = Application::createFrameResource();
  res.addSemaphore("acquire");
  res.addFence("acquire");
  return res;
}

void ApplicationWin::acquireImage(FrameResource& res) {
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

void ApplicationWin::presentFrame(FrameResource& res) {
  presentFrame(res, m_device.getQueue("present"));
}

void ApplicationWin::presentFrame(FrameResource& res, vk::Queue const& queue) {
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

void ApplicationWin::resize(std::size_t width, std::size_t height) {
  m_swap_chain.recreate(vk::Extent2D{uint32_t(width), uint32_t(height)});
  m_camera.setAspect(float(width) / float(height));

  Application::resize(width, height);
}