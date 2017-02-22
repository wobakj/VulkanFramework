#ifndef APPLICATION_CLUSTERED_HPP
#define APPLICATION_CLUSTERED_HPP

#include "application.hpp"

#include "deleter.hpp"
#include "model.hpp"
#include "render_pass.hpp"
#include "frame_buffer.hpp"
#include "frame_resource.hpp"
#include "frame_resource.hpp"

#include <vulkan/vulkan.hpp>

#include <thread>

class ApplicationClustered : public Application {
 public:
  ApplicationClustered(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  ~ApplicationClustered();
  static const uint32_t imageCount;
  
 private:
  void render() override;
  void recordDrawBuffer(FrameResource& res) override;
  FrameResource createFrameResource() override;
  
  void createLights();
  void loadModel();
  void createUniformBuffers();
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
  void createFramebufferAttachments();
  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;
  void updateModel();

  // path to the resource folders
  RenderPass m_render_pass;
  Deleter<VkPipeline> m_pipeline;
  Deleter<VkPipeline> m_pipeline_2;
  FrameBuffer m_framebuffer;
  Model m_model;
  Model m_model_light;
  Deleter<VkDescriptorPool> m_descriptorPool;
  Deleter<VkDescriptorPool> m_descriptorPool_2;
  vk::DescriptorSet m_descriptorSet;
  vk::DescriptorSet m_descriptorSet_3;
  vk::DescriptorSet m_descriptorSet_2;
  Deleter<VkSampler> m_textureSampler;
  std::thread m_thread_load;
  FrameResource m_frame_resource;
};

#endif