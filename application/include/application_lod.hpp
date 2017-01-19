#ifndef APPLICATION_LOD_HPP
#define APPLICATION_LOD_HPP

#include <vulkan/vulkan.hpp>

#include "application_threaded_transfer.hpp"
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

#include "averager.hpp"
#include "timer.hpp"

#include <vector>
#include <atomic>
#include <queue>
#include <thread>

class ApplicationLod : public ApplicationThreadedTransfer {
 public:
  ApplicationLod(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  ~ApplicationLod();
  static cmdline::parser getParser(); 

 private:
  void recordTransferBuffer(FrameResource& res) override;
  void recordDrawBuffer(FrameResource& res) override;
  void updateCommandBuffers(FrameResource& res) override;
  void updateDescriptors(FrameResource& resource) override;
  FrameResource createFrameResource() override;
  
  void createLights();
  void loadModel();
  void createUniformBuffers();
  void createVertexBuffer(std::string const& lod_path, std::size_t cur_budged, std::size_t upload_budget);

  void createTextureImage();
  void createTextureSampler();

  void updateView() override;

  void createFramebuffers() override;
  void createRenderPasses() override;
  void createMemoryPools() override;
  void createPipelines() override;
  void createDescriptorPools() override;
  void createFramebufferAttachments() override;
  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;
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

  bool m_setting_wire;
  bool m_setting_transparent;
  bool m_setting_shaded;
  bool m_setting_levels;
};

#endif