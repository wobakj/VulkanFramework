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
#include "node/node_model.hpp"
#include "scenegraph.hpp"
#include "navigation.hpp"

#include <atomic>
#include <thread>

class ApplicationScenegraph : public ApplicationSingle {
 public:
  ApplicationScenegraph(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  ~ApplicationScenegraph();
  static const uint32_t imageCount;
  
 private:
  void logic() override;
  void recordDrawBuffer(FrameResource& res) override;
  void updateResourceCommandBuffers(FrameResource& res);
  FrameResource createFrameResource() override;
  void updatePipelines() override;
  void updateDescriptors() override;

  void createVertexBuffer();
  void onResize(std::size_t width, std::size_t height) override;

  // void updateView() override;
  void createFramebuffers();
  void createRenderPasses();
  void createPipelines();
  void createDescriptorPools();
  void createFramebufferAttachments();
  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;
  static void mouseCallback(GLFWwindow* window, int button, int action, int mods);
  void updateModel();

  void startTargetNavigation();
  void navigateToTarget();

  // path to the resource folders
  RenderPass m_render_pass;
  FrameBuffer m_framebuffer;
  Geometry m_model;
  ApplicationInstance m_instance;
  ModelLoader m_model_loader;
  Renderer m_renderer;
  Scenegraph m_graph;
  std::map<std::string, ModelNode> m_nodes;
  bool m_selection_phase_flag;
  bool m_target_navi_phase_flag;
  bool m_navi_phase_flag;
  bool m_manipulation_phase_flag;
  double m_target_navi_start;
  double m_target_navi_duration;
  Node* m_curr_hit;
  Navigation m_navigator;

};

#endif