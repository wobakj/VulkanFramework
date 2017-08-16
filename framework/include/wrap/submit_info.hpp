#ifndef SUBMIT_INFO_HPP
#define SUBMIT_INFO_HPP

// #include "wrap/wrapper.hpp"

#include <vulkan/vulkan.hpp>

class SubmitInfo {
 public:
  SubmitInfo();
  SubmitInfo(SubmitInfo && rhs);
  SubmitInfo(SubmitInfo const& rhs);
  
  SubmitInfo& operator=(SubmitInfo rhs);

  void swap(SubmitInfo& dev);

  void addCommandBuffer(vk::CommandBuffer buffer);
  void setCommandBuffers(std::vector<vk::CommandBuffer> buffers);
  void addWaitSemaphore(vk::Semaphore sema, vk::PipelineStageFlags stage);
  void addSignalSemaphore(vk::Semaphore sema);

  // operators from wrapper
  operator vk::SubmitInfo const&() const {
    return m_info;
  }
  // public, to call in case the conversion operator fails
  vk::SubmitInfo const& get() const {
    return m_info;
  }

 private:
  void updatePtrs();

  vk::SubmitInfo m_info;
  std::vector<vk::Semaphore> m_wait_semas;
  std::vector<vk::PipelineStageFlags> m_wait_stages;
  // void destroy() override;
  std::vector<vk::CommandBuffer> m_command_buffers;
  std::vector<vk::Semaphore> m_signal_semas;

  // Device const* m_device;
};

#endif