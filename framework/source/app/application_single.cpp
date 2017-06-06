#include "app/application_single.hpp"

#include "wrap/surface.hpp"

#include <vulkan/vulkan.hpp>

cmdline::parser ApplicationSingle::getParser() {
  return ApplicationWin::getParser();
}

ApplicationSingle::ApplicationSingle(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :ApplicationWin{resource_path, device, surf, 2, cmd_parse}
{
  m_statistics.addTimer("gpu_draw");
  m_statistics.addTimer("render");
  m_statistics.addTimer("fence_acquire");
  m_statistics.addTimer("fence_draw");
}

void ApplicationSingle::shutDown() {
  m_frame_resource.waitFences();
}

ApplicationSingle::~ApplicationSingle() {
  std::cout << "Average GPU draw time: " << m_statistics.get("gpu_draw") << " milliseconds " << std::endl;
  std::cout << std::endl;
  std::cout << "Average render time: " << m_statistics.get("render") << " milliseconds" << std::endl;
  std::cout << "Average acquire fence time: " << m_statistics.get("fence_acquire") << " milliseconds" << std::endl;
  std::cout << "Average draw fence time: " << m_statistics.get("fence_draw") << " milliseconds" << std::endl;
}

void ApplicationSingle::createFrameResources() {
  m_frame_resource = createFrameResource();
}

void ApplicationSingle::updateCommandBuffers() {
  updateResourceCommandBuffers(m_frame_resource);
}

void ApplicationSingle::updateResourcesDescriptors() {
  updateResourceDescriptors(m_frame_resource);
}

FrameResource ApplicationSingle::createFrameResource() {
  return ApplicationWin::createFrameResource();
}

void ApplicationSingle::render() { 
  acquireImage(m_frame_resource);
  m_statistics.start("render");
  // make sure no command buffer is in use
  m_statistics.start("fence_draw");
  m_frame_resource.fence("draw").wait();
  m_statistics.stop("fence_draw");
  static uint64_t frame = 0;
  ++frame;
  // recordTransferBuffer(m_frame_resource);
  recordDrawBuffer(m_frame_resource);
  
  submitDraw(m_frame_resource);

  presentFrame(m_frame_resource);
  m_statistics.stop("render");
}

void ApplicationSingle::emptyDrawQueue() {
  // no draw queue exists, just wait forcurrent draw
  m_frame_resource.waitFences();
}