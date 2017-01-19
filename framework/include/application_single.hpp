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
  virtual void updateCommandBuffers(FrameResource& res) = 0;
  virtual void updateDescriptors(FrameResource& resource) = 0;
  virtual FrameResource createFrameResource();
  
  virtual void createRenderPasses() = 0;
  virtual void createMemoryPools() = 0;
  virtual void createPipelines() = 0;
  virtual void createFramebuffers() = 0;
  virtual void createDescriptorPools() = 0;
  virtual void createFramebufferAttachments() = 0;

  FrameResource m_frame_resource;
  Statistics m_statistics;

 private:
  void render() override;
};

#endif