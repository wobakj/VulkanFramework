#ifndef APPLICATION_TRANSFER_HPP
#define APPLICATION_TRANSFER_HPP

#include "app/application_threaded.hpp"

class Surface;
class SubmitInfo;

template<typename T>
class ApplicationThreadedTransfer : public ApplicationThreaded<T> {
 public:
  ApplicationThreadedTransfer(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse);
  virtual ~ApplicationThreadedTransfer();

  static cmdline::parser getParser();

 protected:
  virtual FrameResource createFrameResource() override;
  void shutDown() override;
 
  virtual SubmitInfo createDrawSubmitInfo(FrameResource const& res) const override;

 private:
  void render() override;
  void startTransferThread();
  void submitTransfer(FrameResource& res);

  void pushForTransfer(FrameResource& frame);
  FrameResource& pullForTransfer();

  void transfer();
  void transferLoop();
  
  std::mutex m_mutex_transfer_queue;
  std::queue<uint32_t> m_queue_transfer_frames;
  Semaphore m_semaphore_transfer;

  std::atomic<bool> m_should_transfer;
  std::thread m_thread_transfer;
};

#include "app/application_threaded_transfer.inl"

#endif