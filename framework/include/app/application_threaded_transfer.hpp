#ifndef APPLICATION_TRANSFER_HPP
#define APPLICATION_TRANSFER_HPP

#include "app/application_threaded.hpp"

#include <vulkan/vulkan.hpp>

class ApplicationThreadedTransfer : public ApplicationThreaded {
 public:
  ApplicationThreadedTransfer(std::string const& resource_path, Device& device, vk::SurfaceKHR const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  virtual ~ApplicationThreadedTransfer();
  
  static const uint32_t imageCount;

 protected:
  virtual FrameResource createFrameResource() override;
  void startTransferThread();
  void shutDown() override;
 
 private:
  void render() override;
  virtual void recordTransferBuffer(FrameResource& res) = 0;
  void submitTransfer(FrameResource& res);
  void submitDraw(FrameResource& res) override;

  void pushForTransfer(uint32_t frame);
  uint32_t pullForTransfer();

  void transfer();
  virtual void transferLoop();
  std::mutex m_mutex_transfer_queue;
  std::queue<uint32_t> m_queue_transfer_frames;
  Semaphore m_semaphore_transfer;

  std::atomic<bool> m_should_transfer;
  std::thread m_thread_transfer;
};

#endif