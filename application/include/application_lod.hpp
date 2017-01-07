#ifndef APPLICATION_THREADED_HPP
#define APPLICATION_THREADED_HPP

#include <vulkan/vulkan.hpp>

#include "application.hpp"
#include "deleter.hpp"
#include "model.hpp"
#include "model_lod.hpp"
#include "buffer.hpp"
#include "render_pass.hpp"
#include "memory.hpp"
#include "frame_buffer.hpp"
#include "fence.hpp"
#include "frame_resource.hpp"
#include "semaphore.hpp"

#include <vector>
#include <atomic>
#include <queue>
#include <thread>

class ApplicationLod : public Application {
 public:
  ApplicationLod(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*);
  ~ApplicationLod();
  void emptyDrawQueue() override;

 private:
  void render() override;
  void recordDrawBuffer(FrameResource& res) override;
  void createCommandBuffers(FrameResource& res);
  void updateCommandBuffers(FrameResource& res);
  void updateDescriptors(FrameResource& resource);
  
  void createLights();
  void loadModel();
  void createUniformBuffers();
  void createVertexBuffer();
  void createTextureImage();
  void createTextureSampler();

  void resize() override;
  void recreatePipeline() override;
  void updateView() override;

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
  void updateModel();

  // path to the resource folders
  RenderPass m_render_pass;
  Deleter<VkPipeline> m_pipeline;
  Deleter<VkPipeline> m_pipeline_2;
  FrameBuffer m_framebuffer;
  Model m_model_light;
  ModelLod m_model_lod;
  Deleter<VkDescriptorPool> m_descriptorPool;
  Deleter<VkDescriptorPool> m_descriptorPool_2;
  vk::DescriptorSet m_descriptorSet;
  vk::DescriptorSet m_descriptorSet_3;
  vk::DescriptorSet m_descriptorSet_2;
  Deleter<VkSampler> m_textureSampler;

  std::thread m_thread_render;
  std::atomic<bool> m_should_draw;
  std::mutex m_mutex_draw_queue;
  std::mutex m_mutex_record_queue;

  std::vector<FrameResource> m_frame_resources;
  std::queue<uint32_t> m_queue_draw_frames;
  std::queue<uint32_t> m_queue_record_frames;
  Semaphore m_semaphore_draw;
  Semaphore m_semaphore_record;

  bool m_setting_wire;
  bool m_setting_transparent;
  bool m_setting_shaded;
};

#endif