#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "wrap/shader.hpp"
#include "wrap/image.hpp"
#include "wrap/image_res.hpp"
#include "wrap/pipeline.hpp"
#include "wrap/pipeline_cache.hpp"
#include "wrap/buffer.hpp"
#include "wrap/buffer_view.hpp"
#include "wrap/device.hpp"
#include "wrap/swap_chain.hpp"
#include "wrap/descriptor_pool.hpp"
#include "wrap/command_pool.hpp"

#include "allocator_block.hpp"
#include "camera.hpp"
#include "transferrer.hpp"
#include "frame_resource.hpp"

#include "cmdline.h"

#include <map>
#include <mutex>

class FrameResource;
class SubmitInfo;

class Application {
 public:
  // allocate and initialize objects
  Application(std::string const& resource_path, Device& device, cmdline::parser const& cmd_parse);

  // free resources
  virtual ~Application(){};

  // react to key input
  inline virtual void keyCallback(int key, int scancode, int action, int mods) {};
  inline virtual void mouseCallback(double x, double y) {};
  inline virtual void mouseButtonCallback(int button, int action, int mods) {};
  // reload shader source and recreate pipeline
  void updateShaderPrograms();
  // draw all objects
  void frame();
  virtual void resize(std::size_t width, std::size_t height);

  // render remaining recorded frames before pipeline rebuild
  // required for multithreaded rendering
  virtual void emptyDrawQueue() = 0;
  // default parser without arguments
  static cmdline::parser getParser();

  virtual bool shouldClose() const = 0;

 protected:
  // call at construction
  void createRenderResources();
  void recreatePipeline();
  // call at shader reload/pipeline config change
  virtual void createMemoryPools();
  virtual void createCommandPools();
  virtual void createDescriptorPools() {};
  virtual void createPipelines() {};
  virtual void createFramebuffers() {};
  virtual void createFramebufferAttachments() {};
  virtual void createRenderPasses() {};
  virtual void createFrameResources() {};
  virtual void updatePipelines() {};
  virtual void updateCommandBuffers() = 0;
  // callbacks
  virtual void onResize(std::size_t width, std::size_t height);
  virtual void onFrameBegin() {};
  virtual void onFrameEnd() {};

  virtual void logic() {};
  virtual void render() = 0;
  virtual void updateView() {};

  virtual FrameResource createFrameResource();
  virtual void updateDescriptors() {};
  virtual void updateResourcesDescriptors() = 0;
  virtual void updateResourceDescriptors(FrameResource& resource) {};
  // cannot be pure due to template argiment
  virtual void updateResourceCommandBuffers(FrameResource& res) {};
  
  void submitDraw(FrameResource& res);
  virtual void recordDrawBuffer(FrameResource& res) = 0;
  virtual SubmitInfo createDrawSubmitInfo(FrameResource const& res) const;

  std::string m_resource_path; 
  Device& m_device;

  PipelineCache m_pipeline_cache;
  DescriptorPool m_descriptor_pool;

  std::map<std::string, vk::DescriptorSet> m_descriptor_sets;
  std::map<std::string, Shader> m_shaders;
  std::map<std::string, GraphicsPipeline> m_pipelines;
  std::map<std::string, BlockAllocator> m_allocators;
  std::map<std::string, ImageRes> m_images;
  std::map<std::string, Buffer> m_buffers;
  std::map<std::string, BufferView> m_buffer_views;
  std::map<std::string, CommandPool> m_command_pools;
  // below command pools so that it is destroyed before
  Transferrer m_transferrer;

  glm::uvec2 m_resolution;
  // workaround so multithreaded apps can be run with single thread
  virtual void startRenderThread() {};
  virtual void recordTransferBuffer(FrameResource& res) {};
  std::vector<FrameResource> m_frame_resources;
};

#endif