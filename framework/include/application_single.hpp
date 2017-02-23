#ifndef APPLICATION_SINGLE_HPP
#define APPLICATION_SINGLE_HPP

#include "application.hpp"
#include "frame_resource.hpp"
#include "statistics.hpp"

class ApplicationSingle : public Application {
 public:
  ApplicationSingle(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  ~ApplicationSingle();
  static const uint32_t imageCount;

 protected:
  void resize() override;
  void recreatePipeline();
  void shutDown();

  // void recordTransferBuffer(FrameResource& res);
  virtual void recordDrawBuffer(FrameResource& res) = 0;
  virtual FrameResource createFrameResource();
  
  FrameResource m_frame_resource;
  Statistics m_statistics;

 private:
  void render() override;
};

#endif