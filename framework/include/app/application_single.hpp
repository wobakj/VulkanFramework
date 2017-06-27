#ifndef APPLICATION_SINGLE_HPP
#define APPLICATION_SINGLE_HPP

#include "app/application.hpp"
#include "frame_resource.hpp"
#include "statistics.hpp"

class ApplicationSingle : public Application {
 public:
  ApplicationSingle(std::string const& resource_path, Device& device, vk::SurfaceKHR const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  ~ApplicationSingle();
  static const uint32_t imageCount;
  
  void emptyDrawQueue() override;
  // default parser without arguments
  static cmdline::parser getParser();
  
 protected:
  void createFrameResources() override;
  void updateCommandBuffers() override;
  void updateResourcesDescriptors() override;

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