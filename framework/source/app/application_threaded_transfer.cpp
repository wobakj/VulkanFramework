#include "app/application_threaded_transfer.hpp"

// c++ wrapper
#include <vulkan/vulkan.hpp>

// child classes must overwrite
const uint32_t ApplicationThreadedTransfer::imageCount = 4;

cmdline::parser ApplicationThreadedTransfer::getParser() {
  return ApplicationThreaded::getParser();
}

ApplicationThreadedTransfer::ApplicationThreadedTransfer(std::string const& resource_path, Device& device, vk::SurfaceKHR const& surf, GLFWwindow* window, cmdline::parser const& cmd_parse) 
 :ApplicationThreaded{resource_path, device, surf, window, cmd_parse, imageCount - 1}
 ,m_semaphore_transfer{0}
 ,m_should_transfer{true}
{
  createSwapChain(surf, cmd_parse, imageCount);
  
  m_statistics.addTimer("fence_transfer");
  m_statistics.addTimer("sema_transfer");
  m_statistics.addTimer("transfer");

  startTransferThread();
}

ApplicationThreadedTransfer::~ApplicationThreadedTransfer() {
  std::cout << std::endl;
  std::cout << "Average transfer semaphore time: " << m_statistics.get("sema_transfer") << " milliseconds " << std::endl;
  std::cout << "Average transfer fence time: " << m_statistics.get("fence_transfer") << " milliseconds " << std::endl;
  std::cout << "Average transfer time: " << m_statistics.get("transfer") << " milliseconds " << std::endl;
}

void ApplicationThreadedTransfer::startTransferThread() {
  if (!m_thread_transfer.joinable()) {
    m_thread_transfer = std::thread(&ApplicationThreadedTransfer::transferLoop, this);
  }
}

void ApplicationThreadedTransfer::shutDown() {
  // shut down transfer thread
  m_should_transfer = false;
  if(m_thread_transfer.joinable()) {
    m_semaphore_transfer.shutDown();
    m_thread_transfer.join();
  }
  else {
    throw std::runtime_error{"could not join thread"};
  }
  m_device.getQueue("transfer").waitIdle();
 ApplicationThreaded::shutDown();
}

FrameResource ApplicationThreadedTransfer::createFrameResource() {
  auto res = ApplicationThreaded::createFrameResource();
  // separate transfer
  res.addCommandBuffer("transfer", m_command_pools.at("transfer").createBuffer(vk::CommandBufferLevel::ePrimary));
  res.addSemaphore("transfer");
  res.addFence("transfer");
  return res;
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
  recordTransferBuffer(resource_record);
  // submitTransfer(resource_record);
  // wait for last acquisition until acquiring again
  m_statistics.start("fence_acquire");
  resource_record.fence("acquire").wait();
  m_statistics.stop("fence_acquire");
  acquireImage(resource_record);
  // wait for drawing finish until rerecording
  recordDrawBuffer(resource_record);
  // add newly recorded frame for drawing
  pushForTransfer(frame_record);
  m_statistics.stop("record");
}

void ApplicationThreadedTransfer::transfer() {
  m_statistics.start("sema_transfer");
  m_semaphore_transfer.wait();
  m_statistics.stop("sema_transfer");
  m_statistics.start("transfer");
  // allow closing of application
  if (!m_should_transfer) return;
  static std::uint64_t frame = 0;
  ++frame;
  // get frame to transfer
  auto frame_transfer = pullForTransfer();
  // get resource to transfer
  auto& resource_transfer = m_frame_resources.at(frame_transfer);
  submitTransfer(resource_transfer);
  // wait for transfering finish until rerecording
  m_statistics.start("fence_transfer");
  resource_transfer.fence("transfer").wait();
  m_statistics.stop("fence_transfer");
  // make frame avaible for rerecording
  pushForDraw(frame_transfer);
  m_statistics.stop("transfer");
}

void ApplicationThreadedTransfer::pushForTransfer(uint32_t frame) {
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_transfer_queue};
    m_queue_transfer_frames.push(frame);
  }
  m_semaphore_transfer.signal();
}

uint32_t ApplicationThreadedTransfer::pullForTransfer() {
  uint32_t frame_transfer = 0;
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_transfer_queue};
    assert(!m_queue_transfer_frames.empty());
    // get frame to transfer
    frame_transfer = m_queue_transfer_frames.front();
    m_queue_transfer_frames.pop();
  }
  return frame_transfer;
}

void ApplicationThreadedTransfer::submitTransfer(FrameResource& res) {
  std::vector<vk::SubmitInfo> submitInfos(1,vk::SubmitInfo{});

  submitInfos[0].setCommandBufferCount(1);
  submitInfos[0].setPCommandBuffers(&res.command_buffers.at("transfer").get());

  vk::Semaphore signalSemaphores[]{res.semaphores.at("transfer")};
  submitInfos[0].signalSemaphoreCount = 1;
  submitInfos[0].pSignalSemaphores = signalSemaphores;

  res.fences.at("transfer").reset();
  m_device.getQueue("transfer").submit(submitInfos, res.fences.at("transfer"));
}

void ApplicationThreadedTransfer::submitDraw(FrameResource& res) {
  std::vector<vk::SubmitInfo> submitInfos(1,vk::SubmitInfo{});
  // wait on image acquisition and data transfer
  vk::Semaphore waitSemaphores[]{res.semaphore("acquire"), res.semaphores.at("transfer")};
  vk::PipelineStageFlags waitStages[]{vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eDrawIndirect};
  submitInfos[0].setWaitSemaphoreCount(2);
  submitInfos[0].setPWaitSemaphores(waitSemaphores);
  submitInfos[0].setPWaitDstStageMask(waitStages);

  submitInfos[0].setCommandBufferCount(1);
  submitInfos[0].setPCommandBuffers(&res.command_buffers.at("draw").get());

  vk::Semaphore signalSemaphores[]{res.semaphore("draw")};
  submitInfos[0].signalSemaphoreCount = 1;
  submitInfos[0].pSignalSemaphores = signalSemaphores;

  res.fence("draw").reset();
  m_device.getQueue("graphics").submit(submitInfos, res.fence("draw"));
}

void ApplicationThreadedTransfer::transferLoop() {
  while (m_should_transfer) {
    transfer();
  }
}
