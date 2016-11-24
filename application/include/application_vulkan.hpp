#ifndef APPLICATION_VULKAN_HPP
#define APPLICATION_VULKAN_HPP

#include <vulkan/vulkan.hpp>

#include "application.hpp"
#include "deleter.hpp"
#include "model.hpp"
#include "buffer.hpp"
#include "render_pass.hpp"
#include "memory.hpp"
#include "frame_buffer.hpp"

#include <vector>
#include <atomic>
#include <thread>

class ApplicationVulkan : public Application {
 public:
  ApplicationVulkan(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*);

 private:
  void render() override;
  void createLights();
  void updateLights();
  void createSemaphores();
  void createUniformBuffers();
  void createCommandBuffers();
  void updateCommandBuffers();
  void createFramebuffers();
  void createRenderPass();
  void createMemoryPools();
  void createGraphicsPipeline();
  void resize() override;
  void recreatePipeline() override;
  void createVertexBuffer();
  void updateView() override;
  void createDescriptorPool();
  void createTextureImage();
  void createTextureSampler();
  void createDepthResource();
  void loadModel();
  void createPrimaryCommandBuffer(int index_fb);
  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;

  // path to the resource folders
  RenderPass m_render_pass;
  Deleter<VkPipeline> m_pipeline;
  Deleter<VkPipeline> m_pipeline_2;
  FrameBuffer m_framebuffer;
  Deleter<VkSemaphore> m_sema_render_done;
  Model m_model;
  Model m_model_2;
  Deleter<VkDescriptorPool> m_descriptorPool;
  Deleter<VkDescriptorPool> m_descriptorPool_2;
  vk::DescriptorSet m_descriptorSet;
  vk::DescriptorSet m_descriptorSet_3;
  vk::DescriptorSet m_descriptorSet_2;
  Deleter<VkSampler> m_textureSampler;
  Deleter<VkFence> m_fence_draw;
  Deleter<VkFence> m_fence_command;
  std::thread m_thread_load;
  std::atomic<bool> m_model_dirty;
  // Application* m_application;
  bool m_sphere;
};

#endif