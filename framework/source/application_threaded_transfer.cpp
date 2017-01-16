#include "application_threaded_transfer.hpp"

// c++ wrapper
#include <vulkan/vulkan.hpp>

ApplicationThreadedTransfer::ApplicationThreadedTransfer(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window, cmdline::parser const& cmd_parse) 
 :ApplicationThreaded{resource_path, device, chain, window, cmd_parse, 3}
{
  m_statistics.addTimer("fence_transfer");
}

ApplicationThreadedTransfer::~ApplicationThreadedTransfer() {
  std::cout << std::endl;
  std::cout << "Average transfer fence time: " << m_statistics.get("fence_transfer") << " milliseconds " << std::endl;
}

void ApplicationThreadedTransfer::render() {
  m_statistics.start("sema_present");
  m_semaphore_present.wait();
  m_statistics.stop("sema_present");
  
  present();
  m_statistics.start("record");

  static uint64_t frame = 0;
  ++frame;
  // get next frame to record
  auto frame_record = pullForRecord();
  // get resource to record
  auto& resource_record = m_frame_resources.at(frame_record);
  // wait for previous transfer
  m_statistics.start("fence_transfer");
  resource_record.fences.at("transfer").wait();
  m_statistics.stop("fence_transfer");
  recordTransferBuffer(resource_record);
  submitTransfer(resource_record);
  // wait for last acquisition until acquiring again
  m_statistics.start("fence_acquire");
  resource_record.fenceAcquire().wait();
  m_statistics.stop("fence_acquire");
  acquireImage(resource_record);
  // wait for drawing finish until rerecording
  m_statistics.start("fence_draw");
  resource_record.fenceDraw().wait();
  m_statistics.stop("fence_draw");
  recordDrawBuffer(resource_record);
  // add newly recorded frame for drawing
  pushForDraw(frame_record);
  m_statistics.stop("record");
}

void ApplicationThreadedTransfer::submitTransfer(FrameResource& res) {
  std::vector<vk::SubmitInfo> submitInfos(1,vk::SubmitInfo{});

  submitInfos[0].setCommandBufferCount(1);
  submitInfos[0].setPCommandBuffers(&res.command_buffers.at("transfer"));

  vk::Semaphore signalSemaphores[]{res.semaphores.at("transfer")};
  submitInfos[0].signalSemaphoreCount = 1;
  submitInfos[0].pSignalSemaphores = signalSemaphores;

  res.fences.at("transfer").reset();
  m_device.getQueue("transfer").submit(submitInfos, res.fences.at("transfer"));
}

void ApplicationThreadedTransfer::submitDraw(FrameResource& res) {
  std::vector<vk::SubmitInfo> submitInfos(1,vk::SubmitInfo{});
  // wait on image acquisition and data transfer
  vk::Semaphore waitSemaphores[]{res.semaphoreAcquire(), res.semaphores.at("transfer")};
  vk::PipelineStageFlags waitStages[]{vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eDrawIndirect};
  submitInfos[0].setWaitSemaphoreCount(2);
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
