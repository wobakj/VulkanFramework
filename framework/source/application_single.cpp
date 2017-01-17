#include "application_single.hpp"

#include <vulkan/vulkan.hpp>

ApplicationSingle::ApplicationSingle(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window, cmdline::parser const& cmd_parse) 
 :Application{resource_path, device, chain, window, cmd_parse}
{
  m_statistics.addTimer("render");
  m_statistics.addTimer("fence_acquire");
  m_statistics.addTimer("fence_draw");

  m_frame_resource = createFrameResource();  
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

FrameResource ApplicationSingle::createFrameResource() {
  FrameResource res{m_device};
  return res;
}

void ApplicationSingle::render() { 
  // make sure image was acquired
  m_statistics.start("fence_acquire");
  m_frame_resource.fenceAcquire().wait();
  m_statistics.stop("fence_acquire");
  m_statistics.start("render");
  acquireImage(m_frame_resource);
  // make sure no command buffer is in use
  m_statistics.start("fence_draw");
  m_frame_resource.fenceDraw().wait();
  m_statistics.stop("fence_draw");
  static uint64_t frame = 0;
  ++frame;
  // recordTransferBuffer(m_frame_resource);
  recordDrawBuffer(m_frame_resource);
  
  submitDraw(m_frame_resource);

  presentFrame(m_frame_resource);
  m_statistics.stop("render");
}

///////////////////////////// update functions ////////////////////////////////
void ApplicationSingle::resize() {
  m_frame_resource.waitFences();
  createFramebufferAttachments();
  createRenderPasses();
  createFramebuffers();

  recreatePipeline();
}

void ApplicationSingle::recreatePipeline() {
  // make sure pipeline is free before rebuilding
  m_frame_resource.waitFences();
  createPipelines();
  createDescriptorPools();

  updateDescriptors(m_frame_resource);
  updateCommandBuffers(m_frame_resource);
}