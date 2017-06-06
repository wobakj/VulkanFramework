#ifndef APPLICATION_SINGLE_HPP
#define APPLICATION_SINGLE_HPP

#include "app/application_win.hpp"
#include "frame_resource.hpp"

class Surface;

class ApplicationSingle : public ApplicationWin {
 public:
  ApplicationSingle(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse);
  ~ApplicationSingle();
  
  
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

 private:
  void render() override;

};

#endif