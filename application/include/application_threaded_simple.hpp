#ifndef APPLICATION_THREADED_SIMPLE_HPP
#define APPLICATION_THREADED_SIMPLE_HPP

#include <vulkan/vulkan.hpp>

#include "application_threaded.hpp"
#include "deleter.hpp"
#include "model.hpp"
#include "buffer.hpp"
#include "render_pass.hpp"
#include "memory.hpp"
#include "frame_buffer.hpp"
#include "fence.hpp"

#include <vector>
#include <atomic>

class ApplicationThreadedSimple : public ApplicationThreaded {
 public:
  ApplicationThreadedSimple(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*);
  ~ApplicationThreadedSimple();

 private:
  void render() override;
  void recordDrawBuffer(FrameResource& res) override;
  void createCommandBuffers(FrameResource& res) override;
  void updateCommandBuffers(FrameResource& res) override;
  void updateDescriptors(FrameResource& resource) override;
  
  void createLights();
  void loadModel();
  void createUniformBuffers();
  void createVertexBuffer();
  void createTextureImage();
  void createTextureSampler();

  void updateView() override;

  void createFramebuffers() override;
  void createFramebufferAttachments() override;
  void createRenderPasses() override;
  void createMemoryPools() override;
  void createPipelines() override;
  void createDescriptorPools() override;
  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;
  FrameResource createFrameResource() override;
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

  bool m_sphere;
  std::thread m_thread_load;
  std::atomic<bool> m_model_dirty;
};

#endif