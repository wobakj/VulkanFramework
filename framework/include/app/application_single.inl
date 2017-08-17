#include "wrap/submit_info.hpp"

#include "frame_resource.hpp"

#include "cmdline.h"

#include <vulkan/vulkan.hpp>

template<typename T>
cmdline::parser ApplicationSingle<T>::getParser() {
  return T::getParser();
}

template<typename T>
ApplicationSingle<T>::ApplicationSingle(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :T{resource_path, device, surf, 2, cmd_parse}
{
  this->m_statistics.addTimer("gpu_draw");
  this->m_statistics.addTimer("render");
  this->m_statistics.addTimer("fence_acquire");
  this->m_statistics.addTimer("fence_draw");
}

template<typename T>
void ApplicationSingle<T>::shutDown() {
  // must wait on queue before waiting on fences, otherwise device is lost
  this->m_device.getQueue("graphics").waitIdle();
  this->m_frame_resources.front().waitFences();
}

template<typename T>
ApplicationSingle<T>::~ApplicationSingle() {
  std::cout << "Average GPU draw time: " << this->m_statistics.get("gpu_draw") << " milliseconds " << std::endl;
  std::cout << std::endl;
  std::cout << "Average render time: " << this->m_statistics.get("render") << " milliseconds" << std::endl;
  std::cout << "Average acquire fence time: " << this->m_statistics.get("fence_acquire") << " milliseconds" << std::endl;
  std::cout << "Average draw fence time: " << this->m_statistics.get("fence_draw") << " milliseconds" << std::endl;
}

template<typename T>
FrameResource ApplicationSingle<T>::createFrameResource() {
  auto res = T::createFrameResource();
  res.setCommandBuffer("transfer", std::move(this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::ePrimary)));
  res.addSemaphore("transfer");
  return res;
}

template<typename T>
void ApplicationSingle<T>::render() { 

  this->acquireImage(this->m_frame_resources.front());
  this->m_statistics.start("render");
  // make sure no command buffer is in use
  this->m_statistics.start("fence_draw");
  this->m_frame_resources.front().fence("draw").wait();
  this->m_statistics.stop("fence_draw");
  static uint64_t frame = 0;
  ++frame;
  this->recordTransferBuffer(this->m_frame_resources.front());
  this->recordDrawBuffer(this->m_frame_resources.front());
  
  this->submitDraw(this->m_frame_resources.front());

  this->presentFrame(this->m_frame_resources.front());
  this->m_statistics.stop("render");
}

template<typename T>
SubmitInfo ApplicationSingle<T>::createDrawSubmitInfo(FrameResource const& res) const {
  SubmitInfo info = T::createDrawSubmitInfo(res);
  info.addCommandBuffer(res.command_buffers.at("transfer").get());
  return info;
}

template<typename T>
void ApplicationSingle<T>::emptyDrawQueue() {
  // no draw queue exists, just wait for current draw
  this->m_frame_resources.front().waitFences();
}