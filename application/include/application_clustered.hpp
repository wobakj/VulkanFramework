#ifndef APPLICATION_CLUSTERED_HPP
#define APPLICATION_CLUSTERED_HPP

#include "application_single.hpp"

#include "deleter.hpp"
#include "model.hpp"
#include "render_pass.hpp"
#include "frame_buffer.hpp"
#include "frame_resource.hpp"
#include "pipeline_info.hpp"
#include "pipeline.hpp"

#include <vulkan/vulkan.hpp>

#include <thread>

class ApplicationClustered : public ApplicationSingle {
 public:
  ApplicationClustered(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  ~ApplicationClustered();
  static cmdline::parser getParser(); 
  static const uint32_t imageCount;
  
 private:
  void logic() override;
  void recordDrawBuffer(FrameResource& res) override;
  FrameResource createFrameResource() override;
  void updateResourceCommandBuffers(FrameResource& res) override;
  void updatePipelines() override;
  
  void createLights();
  void loadModel();
  void createUniformBuffers();
  void createVertexBuffer();
  void createTextureImages();
  void createTextureSamplers();
  void updateLightVolume();

  void updateView() override;

  void createFramebuffers() override;
  void createRenderPasses() override;
  void createMemoryPools() override;
  void createPipelines() override;
  void createDescriptorPools() override;
  void createFramebufferAttachments() override;
  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;

  // path to the resource folders
  RenderPass m_render_pass;
  FrameBuffer m_framebuffer;
  Model m_model;
  Deleter<VkDescriptorPool> m_descriptorPool;
  Deleter<VkDescriptorPool> m_descriptorPool_2;
  Deleter<VkSampler> m_textureSampler;
  Deleter<VkSampler> m_volumeSampler;
  std::thread m_thread_load;

  std::vector<uint32_t> m_data_light_volume;
};

#endif