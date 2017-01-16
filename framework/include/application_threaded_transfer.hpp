#ifndef APPLICATION_TRANSFER_HPP
#define APPLICATION_TRANSFER_HPP

#include "application_threaded.hpp"

#include <vulkan/vulkan.hpp>

class ApplicationThreadedTransfer : public ApplicationThreaded {
 public:
  ApplicationThreadedTransfer(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  virtual ~ApplicationThreadedTransfer();

 private:
  void render() override;
  virtual void recordTransferBuffer(FrameResource& res) = 0;
  void submitTransfer(FrameResource& res);
  void submitDraw(FrameResource& res) override;

};

#endif