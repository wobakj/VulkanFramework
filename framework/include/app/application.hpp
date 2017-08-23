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
#include "statistics.hpp"

#include <map>
#include <mutex>

namespace cmdline {
  class parser;
}

class FrameResource;
class SubmitInfo;

// TODO: check if memory copies of buffers use required device mem or data size
class Application {
 public:
  // allocate and initialize objects
  Application(std::string const& resource_path, Device& device, uint32_t num_frames, cmdline::parser const& cmd_parse);

  // free resources
  virtual ~Application(){};

  // react to key input
  inline virtual void keyCallback(int key, int scancode, int action, int mods) {};
  inline virtual void mouseCallback(double x, double y) {};
  inline virtual void mouseButtonCallback(int button, int action, int mods) {};
  // reload shader source and recreate pipeline
  void updateShaderPrograms();
  // frame step
  void frame();
  // overwritten by win/worker app classes
  virtual void resize(std::size_t width, std::size_t height);

  // render remaining recorded frames before pipeline is rebuilt
  // required for multithreaded rendering
  virtual void emptyDrawQueue() = 0;
  // default parser without arguments
  static cmdline::parser getParser();

  virtual bool shouldClose() const = 0;

 protected:
  // call at construction
  void createRenderResources();
  void recreatePipeline();
  void submitDraw(FrameResource& res);
  // initialisation methods
  virtual void createMemoryPools();
  virtual void createCommandPools();
  virtual void createDescriptorPools() {};
  virtual void createPipelines() {};
  virtual void createFramebuffers() {};
  virtual void createFramebufferAttachments() {};
  virtual void createRenderPasses() {};
  virtual FrameResource createFrameResource();
  // overwritten by abstract apps
  virtual void onFrameBegin() {};
  virtual void onFrameEnd() {};
  virtual SubmitInfo createDrawSubmitInfo(FrameResource const& res) const;
  virtual void render() = 0;
  // update methods
  virtual void updateDescriptors() {};
  virtual void updatePipelines() {};
  virtual void updateResourceDescriptors(FrameResource& resource) {};
  // cannot be pure due to template argument
  virtual void updateResourceCommandBuffers(FrameResource& res) {};
  // overwritten by actual apps
  virtual void onResize() {};
  virtual void logic() {};
  virtual void recordDrawBuffer(FrameResource& res) = 0;

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

  glm::u32vec2 m_resolution;
  Statistics m_statistics;

  // workaround so multithreaded apps can be run with single thread
  virtual void recordTransferBuffer(FrameResource& res) {};
  std::vector<FrameResource> m_frame_resources;

 private:
  void updateCommandBuffers();
  void updateResourcesDescriptors();
  void createFrameResources();
};

#endif