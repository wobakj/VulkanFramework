#ifndef APPLICATION_RENDERER_HPP
#define APPLICATION_RENDERER_HPP

#include "app/application_single.hpp"

#include "deleter.hpp"
#include "geometry.hpp"
#include "wrap/render_pass.hpp"
#include "wrap/frame_buffer.hpp"
#include "wrap/sampler.hpp"
#include "ren/application_instance.hpp"
#include "ren/model_loader.hpp"
#include "ren/renderer.hpp"
#include "ren/node.hpp"

#include <atomic>
#include <thread>

class ApplicationRenderer : public ApplicationSingle {
 public:
  ApplicationRenderer(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  ~ApplicationRenderer();
  static const uint32_t imageCount;
  
 private:
  void logic() override;
  void recordDrawBuffer(FrameResource& res) override;
  void updateResourceCommandBuffers(FrameResource& res);
  FrameResource createFrameResource() override;
  void updatePipelines() override;
  void updateDescriptors() override;
  
  void createLights();
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
  Sampler m_sampler;
  ApplicationInstance m_instance;
  ModelLoader m_model_loader;
  Renderer m_renderer;
  std::map<std::string, Node> m_nodes;
};

#endif