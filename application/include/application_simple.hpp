#ifndef APPLICATION_SIMPLE_HPP
#define APPLICATION_SIMPLE_HPP

#include "app/application_win.hpp"
#include "app/application_single.hpp"

#include "deleter.hpp"
#include "geometry.hpp"
#include "wrap/render_pass.hpp"
#include "wrap/frame_buffer.hpp"
#include "wrap/sampler.hpp"

#include <atomic>
#include <thread>

template<typename T>
class ApplicationSimple : public T {
 public:
  ApplicationSimple(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse);
  ~ApplicationSimple();

  static cmdline::parser getParser(); 

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

  void updateView();
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
  Sampler m_sampler;
};

#include "application_simple.inl"

#endif