#ifndef APPLICATION_THREADED_SIMPLE_HPP
#define APPLICATION_THREADED_SIMPLE_HPP

#include "app/application_win.hpp"
#include "app/application_threaded.hpp"

#include "deleter.hpp"
#include "geometry.hpp"
#include "wrap/render_pass.hpp"
#include "wrap/frame_buffer.hpp"
#include "wrap/sampler.hpp"

#include <vector>
#include <atomic>

class ApplicationThreadedSimple : public ApplicationThreaded<ApplicationWin> {
 public:
  ApplicationThreadedSimple(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse);
  ~ApplicationThreadedSimple();

  static cmdline::parser getParser(); 

 private:
  void logic() override;
  void recordDrawBuffer(FrameResource& res) override;
  void updateResourceCommandBuffers(FrameResource& res) override;
  void updateResourceDescriptors(FrameResource& resource) override;
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

  void createFramebuffers() override;
  void createFramebufferAttachments() override;
  void createRenderPasses() override;
  void createPipelines() override;
  void createDescriptorPools() override;
  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;
  void updateModel();

  // path to the resource folders
  RenderPass m_render_pass;
  FrameBuffer m_framebuffer;
  Geometry m_model;
  Geometry m_model_2;
  Sampler m_sampler;

  bool m_sphere;
  std::thread m_thread_load;
  std::atomic<bool> m_model_dirty;
};

#endif