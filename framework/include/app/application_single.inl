// #include "app/application_single.hpp"

#include "wrap/surface.hpp"
#include "wrap/submit_info.hpp"

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
  m_frame_resource.waitFences();
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
void ApplicationSingle<T>::createFrameResources() {
  m_frame_resource = this->createFrameResource();
}

template<typename T>
void ApplicationSingle<T>::updateCommandBuffers() {
  this->updateResourceCommandBuffers(m_frame_resource);
}

template<typename T>
void ApplicationSingle<T>::updateResourcesDescriptors() {
  this->updateResourceDescriptors(m_frame_resource);
}

template<typename T>
void ApplicationSingle<T>::render() { 
  this->acquireImage(m_frame_resource);
  this->m_statistics.start("render");
  // make sure no command buffer is in use
  this->m_statistics.start("fence_draw");
  m_frame_resource.fence("draw").wait();
  this->m_statistics.stop("fence_draw");
  static uint64_t frame = 0;
  ++frame;
  // recordTransferBuffer(m_frame_resource);
  this->recordDrawBuffer(m_frame_resource);
  
  this->submitDraw(m_frame_resource);

  this->presentFrame(m_frame_resource);
  this->m_statistics.stop("render");
}

template<typename T>
void ApplicationSingle<T>::emptyDrawQueue() {
  // no draw queue exists, just wait forcurrent draw
  m_frame_resource.waitFences();
}