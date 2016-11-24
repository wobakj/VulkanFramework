#ifndef APPLICATION_VULKAN_HPP
#define APPLICATION_VULKAN_HPP

#include "application.hpp"

#include "deleter.hpp"
#include "model.hpp"
#include "render_pass.hpp"
#include "frame_buffer.hpp"
#include "frame_resource.hpp"

#include <vulkan/vulkan.hpp>

#include <atomic>
#include <thread>

class ApplicationVulkan : public Application {
 public:
  ApplicationVulkan(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*);
  ~ApplicationVulkan();
 private:
  void render() override;
  void recordDrawBuffer(FrameResource& res) override;
  
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
  void updateModel();

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
  std::thread m_thread_load;
  std::atomic<bool> m_model_dirty;
  bool m_sphere;
  FrameResource m_frame_resource;
};

#endif