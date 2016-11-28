#ifndef APPLICATION_THREADED_HPP
#define APPLICATION_THREADED_HPP

#include <vulkan/vulkan.hpp>

#include "application.hpp"
#include "deleter.hpp"
#include "model.hpp"
#include "buffer.hpp"
#include "render_pass.hpp"
#include "memory.hpp"
#include "frame_buffer.hpp"
#include "fence.hpp"
#include "frame_resource.hpp"

#include <vector>
#include <atomic>
#include <queue>
#include <thread>

class ApplicationThreaded : public Application {
 public:
  ApplicationThreaded(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*);
  ~ApplicationThreaded();

 private:
  void render() override;
  void createPrimaryCommandBuffer(FrameResource& res);
  
  void createLights();
  void updateLights();
  void loadModel();
  void createUniformBuffers();
  void createCommandBuffers(FrameResource& res);
  void createVertexBuffer();
  void createTextureImage();
  void createTextureSampler();

  void resize() override;
  void recreatePipeline() override;
  void updateView() override;
  void updateCommandBuffers(FrameResource& res);
  void createFramebuffers();
  void createRenderPass();
  void createMemoryPools();
  void createGraphicsPipeline();
  void createDescriptorPool();
  void createDepthResource();
  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;
  void drawLoop();
  void draw();
  void createFrameResources();
  void emptyDrawQueue();

  // path to the resource folders
  RenderPass m_render_pass;
  Deleter<VkPipeline> m_pipeline;
  Deleter<VkPipeline> m_pipeline_2;
  FrameBuffer m_framebuffer;
  Model m_model;
  Model m_model_2;
  Deleter<VkDescriptorPool> m_descriptorPool;
  Deleter<VkDescriptorPool> m_descriptorPool_2;
  vk::DescriptorSet m_descriptorSet;
  vk::DescriptorSet m_descriptorSet_3;
  vk::DescriptorSet m_descriptorSet_2;
  Deleter<VkSampler> m_textureSampler;

  bool m_sphere;
  bool m_initializing;

  std::thread m_thread_load;
  std::thread m_thread_render;
  std::atomic<bool> m_model_dirty;
  std::atomic<bool> m_should_draw;
  std::mutex m_mutex_draw_queue;
  std::mutex m_mutex_record_queue;

  std::vector<FrameResource> m_frame_resources;
  std::queue<uint32_t> m_queue_draw_frames;
  std::queue<uint32_t> m_queue_record_frames;
  void acquireImage(FrameResource& res);
  void present(FrameResource& res);
  void submitDraw(FrameResource& res);
};

#endif