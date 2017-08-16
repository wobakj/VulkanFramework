#include "wrap/submit_info.hpp"

SubmitInfo::SubmitInfo()
 :m_info{}
{}

SubmitInfo::SubmitInfo(SubmitInfo && rhs)
 :SubmitInfo{}
{
  swap(rhs);
}

SubmitInfo::SubmitInfo(SubmitInfo const& rhs)
 :m_wait_semas{rhs.m_wait_semas}
 ,m_wait_stages{rhs.m_wait_stages}
 ,m_signal_semas{rhs.m_signal_semas}
{
  updatePtrs();
}

SubmitInfo& SubmitInfo::operator=(SubmitInfo rhs) {
  swap(rhs);
  return *this;
}

void SubmitInfo::updatePtrs() {
  m_info.pWaitSemaphores = m_wait_semas.data();
  m_info.pWaitDstStageMask = m_wait_stages.data();
  m_info.waitSemaphoreCount = uint32_t(m_wait_semas.size());

  m_info.pCommandBuffers = m_command_buffers.data();
  m_info.commandBufferCount = uint32_t(m_command_buffers.size());

  m_info.pSignalSemaphores = m_signal_semas.data();
  m_info.signalSemaphoreCount = uint32_t(m_signal_semas.size());
}

void SubmitInfo::setCommandBuffers(std::vector<vk::CommandBuffer> buffers) {
  m_command_buffers = buffers;
  updatePtrs();
}


void SubmitInfo::addCommandBuffer(vk::CommandBuffer buffer) {
  m_command_buffers.emplace_back(buffer);
  updatePtrs();
}

void SubmitInfo::addWaitSemaphore(vk::Semaphore sema, vk::PipelineStageFlags stage) {
  m_wait_semas.emplace_back(sema);
  m_wait_stages.emplace_back(stage);
  updatePtrs();
}

void SubmitInfo::addSignalSemaphore(vk::Semaphore sema) {
  m_signal_semas.emplace_back(sema);
  updatePtrs();
}

void SubmitInfo::swap(SubmitInfo& rhs) {
  std::swap(m_wait_semas, rhs.m_wait_semas);
  std::swap(m_wait_stages, rhs.m_wait_stages);
  std::swap(m_signal_semas, rhs.m_signal_semas);
  updatePtrs();
}