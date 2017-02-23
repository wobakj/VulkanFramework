#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "shader.hpp"
#include "image.hpp"
#include "pipeline.hpp"
#include "buffer.hpp"
#include "buffer_view.hpp"
#include "camera.hpp"
#include "device.hpp"
#include "swap_chain.hpp"
#include "cmdline.h"

#include <map>
#include <mutex>

class FrameResource;

class Application {
 public:
  // allocate and initialize objects
  Application(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  // free resources
  virtual ~Application(){};

  // react to key input
  inline virtual void keyCallback(int key, int scancode, int action, int mods) {};
  // 
  void updateShaderPrograms();
  // draw all objects
  void frame();
  void resize(std::size_t with, std::size_t height);

  // render remaining recorded frames before pipeline rebuild
  // required for multithreaded rendering
  virtual void emptyDrawQueue() {};
  // default parser without arguments
  static cmdline::parser getParser() {
    return cmdline::parser{};
  }; 

  static const uint32_t imageCount;

 protected:
  virtual void createMemoryPools() = 0;
  virtual void createPipelines() = 0;
  virtual void updatePipelines();
  virtual void createFramebuffers() = 0;
  virtual void createFramebufferAttachments() = 0;
  virtual void createDescriptorPools() {};
  virtual void createRenderPasses() = 0;

  virtual void update() {};
  virtual void render() = 0;
  virtual void updateView() {};
  // called on shader reload
  virtual void recreatePipeline() = 0;
  // called on resize
  virtual void resize() = 0;
  virtual FrameResource createFrameResource();
  virtual void updateDescriptors(FrameResource& resource) {};
  virtual void updateCommandBuffers(FrameResource& res) = 0;
  
  void acquireImage(FrameResource& res);
  virtual void presentFrame(FrameResource& res);
  virtual void presentFrame(FrameResource& res, vk::Queue const&);
  virtual void submitDraw(FrameResource& res);
  virtual void recordDrawBuffer(FrameResource& res) = 0;

  std::string m_resource_path; 

  // container for the shader programs
  Camera m_camera;
  Device& m_device;
  SwapChain const& m_swap_chain;

  std::map<std::string, vk::DescriptorSet> m_descriptor_sets;
  std::map<std::string, Shader> m_shaders;
  std::map<std::string, Pipeline> m_pipelines;
  std::map<std::string, Image> m_images;
  std::map<std::string, Buffer> m_buffers;
  std::map<std::string, BufferView> m_buffer_views;

 private:
};

#endif