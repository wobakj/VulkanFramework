// #include "app/application_single.hpp"

#include "wrap/surface.hpp"

#include <vulkan/vulkan.hpp>
template<typename T>
cmdline::parser ApplicationSingle<T>::getParser() {
  return T::getParser();
}
template<typename T>
ApplicationSingle<T>::ApplicationSingle(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :T{resource_path, device, surf, 2, cmd_parse}
{
  T::m_statistics.addTimer("gpu_draw");
  T::m_statistics.addTimer("render");
  T::m_statistics.addTimer("fence_acquire");
  T::m_statistics.addTimer("fence_draw");
}
template<typename T>
void ApplicationSingle<T>::shutDown() {
  m_frame_resource.waitFences();
}
template<typename T>
ApplicationSingle<T>::~ApplicationSingle() {
  std::cout << "Average GPU draw time: " << T::m_statistics.get("gpu_draw") << " milliseconds " << std::endl;
  std::cout << std::endl;
  std::cout << "Average render time: " << T::m_statistics.get("render") << " milliseconds" << std::endl;
  std::cout << "Average acquire fence time: " << T::m_statistics.get("fence_acquire") << " milliseconds" << std::endl;
  std::cout << "Average draw fence time: " << T::m_statistics.get("fence_draw") << " milliseconds" << std::endl;
}
template<typename T>
void ApplicationSingle<T>::createFrameResources() {
  m_frame_resource = createFrameResource();
}
template<typename T>
void ApplicationSingle<T>::updateCommandBuffers() {
  updateResourceCommandBuffers(m_frame_resource);
}
template<typename T>
void ApplicationSingle<T>::updateResourcesDescriptors() {
  updateResourceDescriptors(m_frame_resource);
}
template<typename T>
FrameResource ApplicationSingle<T>::createFrameResource() {
  return T::createFrameResource();
}

template<typename T>
void ApplicationSingle<T>::render() { 
  T::acquireImage(m_frame_resource);
  T::m_statistics.start("render");
  // make sure no command buffer is in use
  T::m_statistics.start("fence_draw");
  m_frame_resource.fence("draw").wait();
  T::m_statistics.stop("fence_draw");
  static uint64_t frame = 0;
  ++frame;
  // recordTransferBuffer(m_frame_resource);
  recordDrawBuffer(m_frame_resource);
  
  T::submitDraw(m_frame_resource);

  T::presentFrame(m_frame_resource);
  T::m_statistics.stop("render");
}
template<typename T>
void ApplicationSingle<T>::emptyDrawQueue() {
  // no draw queue exists, just wait forcurrent draw
  m_frame_resource.waitFences();
}