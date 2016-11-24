#ifndef APPLICATION_LOD_HPP
#define APPLICATION_LOD_HPP

#include "application.hpp"

#include "deleter.hpp"
#include "model.hpp"
#include "model_lod.hpp"
#include "render_pass.hpp"
#include "frame_buffer.hpp"

#include <vulkan/vulkan.hpp>

#include <atomic>
#include <thread>

class ApplicationVulkan : public Application {
 public:
  ApplicationVulkan(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*);

 private:
  void render() override;
  void createPrimaryCommandBuffer(int index_fb);
  
  void createLights();
  void updateLights();
  void loadModel();
  void createUniformBuffers();
  void createCommandBuffers();
  void createVertexBuffer();
  void createTextureImage();
  void createTextureSampler();

  void resize() override;
  void recreatePipeline() override;
  void updateView() override;
  void updateCommandBuffers();
  void createFramebuffers();
  void createRenderPass();
  void createMemoryPools();
  void createGraphicsPipeline();
  void createDescriptorPool();
  void createDepthResource();
  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;

  // path to the resource folders
  RenderPass m_render_pass;
  Deleter<VkPipeline> m_pipeline;
  Deleter<VkPipeline> m_pipeline_2;
  FrameBuffer m_framebuffer;
  Model m_model;
  ModelLod m_model_lod;
  Deleter<VkDescriptorPool> m_descriptorPool;
  Deleter<VkDescriptorPool> m_descriptorPool_2;
  vk::DescriptorSet m_descriptorSet;
  vk::DescriptorSet m_descriptorSet_3;
  vk::DescriptorSet m_descriptorSet_2;
  Deleter<VkSampler> m_textureSampler;
  // Deleter<VkFence> m_fence_draw;
  std::thread m_thread_load;
  std::atomic<bool> m_model_dirty;
  // Application* m_application;
  bool m_sphere;
  bool m_initializing;
};

#endif