#ifndef APPLICATION_VULKAN_HPP
#define APPLICATION_VULKAN_HPP

#include "application_single.hpp"

#include "deleter.hpp"
#include "model.hpp"
#include "render_pass.hpp"
#include "frame_buffer.hpp"
#include "frame_resource.hpp"

#include <vulkan/vulkan.hpp>

#include <atomic>
#include <thread>

class ApplicationVulkan : public ApplicationSingle {
 public:
  ApplicationVulkan(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  ~ApplicationVulkan();
  static const uint32_t imageCount;
  
 private:
  void render() override;
  void recordDrawBuffer(FrameResource& res) override;
  FrameResource createFrameResource() override;
  void updatePipelines() override;
  
  void createLights();
  void loadModel();
  void createUniformBuffers();
  void createVertexBuffer();
  void createTextureImage();
  void createTextureSampler();

  void updateView() override;
  void updateCommandBuffers(FrameResource& res);
  void createFramebuffers();
  void createRenderPasses();
  void createMemoryPools();
  void createPipelines();
  void createDescriptorPools();
  void createFramebufferAttachments();
  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;
  void updateModel();

  // path to the resource folders
  RenderPass m_render_pass;
  FrameBuffer m_framebuffer;
  Model m_model;
  Model m_model_2;
  Deleter<VkDescriptorPool> m_descriptorPool;
  Deleter<VkDescriptorPool> m_descriptorPool_2;
  Deleter<VkSampler> m_textureSampler;
  std::thread m_thread_load;
  std::atomic<bool> m_model_dirty;
  bool m_sphere;
};

#endif