#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "wrap/shader.hpp"
#include "wrap/image.hpp"
#include "wrap/pipeline.hpp"
#include "wrap/pipeline_cache.hpp"
#include "wrap/buffer.hpp"
#include "wrap/buffer_view.hpp"
#include "wrap/device.hpp"
#include "wrap/swap_chain.hpp"
#include "wrap/descriptor_pool.hpp"
#include "block_allocator.hpp"
#include "camera.hpp"
#include "transferrer.hpp"

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
  void resize(std::size_t width, std::size_t height);
  virtual void onResize(std::size_t width, std::size_t height);

  // render remaining recorded frames before pipeline rebuild
  // required for multithreaded rendering
  virtual void emptyDrawQueue() = 0;
  // default parser without arguments
  static cmdline::parser getParser() {
    return cmdline::parser{};
  }; 

  static const uint32_t imageCount;

 protected:
  // call at construction
  void createRenderResources();
  // call at shader reload/pipeline config change
  virtual void recreatePipeline();
  virtual void createDescriptorPools() {};
  virtual void createMemoryPools();
  virtual void createPipelines() = 0;
  virtual void createFramebuffers() = 0;
  virtual void createFramebufferAttachments() = 0;
  virtual void createRenderPasses() = 0;
  virtual void createFrameResources() = 0;
  virtual void updatePipelines() = 0;
  virtual void updateCommandBuffers() = 0;

  virtual void logic() {};
  virtual void render() = 0;
  virtual void updateView() {};

  virtual FrameResource createFrameResource();
  virtual void updateDescriptors() {};
  virtual void updateResourcesDescriptors() = 0;
  virtual void updateResourceDescriptors(FrameResource& resource) {};
  virtual void updateResourceCommandBuffers(FrameResource& res) = 0;
  
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
  PipelineCache m_pipeline_cache;
  DescriptorPool m_descriptor_pool;
  Transferrer m_transferrer;

  std::map<std::string, vk::DescriptorSet> m_descriptor_sets;
  std::map<std::string, Shader> m_shaders;
  std::map<std::string, GraphicsPipeline> m_pipelines;
  std::map<std::string, BlockAllocator> m_allocators;
  std::map<std::string, Image> m_images;
  std::map<std::string, Buffer> m_buffers;
  std::map<std::string, BufferView> m_buffer_views;
};

#endif