#ifndef APPLICATION_VULKAN_HPP
#define APPLICATION_VULKAN_HPP

#include "app/application_single.hpp"

#include "deleter.hpp"
#include "geometry.hpp"
#include "wrap/render_pass.hpp"
#include "wrap/frame_buffer.hpp"
#include "ren/texture_database.hpp"
#include "ren/material_database.hpp"

#include <atomic>
#include <thread>

class ApplicationVulkan : public ApplicationSingle {
 public:
  ApplicationVulkan(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  ~ApplicationVulkan();
  static const uint32_t imageCount;
  
 private:
  void logic() override;
  void recordDrawBuffer(FrameResource& res) override;
  void updateResourceCommandBuffers(FrameResource& res);
  FrameResource createFrameResource() override;
  void updatePipelines() override;
  void updateDescriptors() override;
  
  void createLights();
  void loadModel();
  void createUniformBuffers();
  void createVertexBuffer();
  void createTextureImage();
  void createTextureSampler();

  void updateView() override;
  void createFramebuffers();
  void createRenderPasses();
  void createPipelines();
  void createDescriptorPools();
  void createFramebufferAttachments();
  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;
  void updateModel();

  // path to the resource folders
  RenderPass m_render_pass;
  FrameBuffer m_framebuffer;
  Geometry m_model;
  Geometry m_model_2;
  Deleter<VkSampler> m_textureSampler;
  std::thread m_thread_load;
  std::atomic<bool> m_model_dirty;
  bool m_sphere;
  TextureDatabase m_database_tex;
  MaterialDatabase m_database_mat;
};

#endif