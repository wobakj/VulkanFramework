#ifndef APPLICATION_REN_CLUSTER_HPP
#define APPLICATION_REN_CLUSTER_HPP

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

class ApplicationScenegraphClustered : public ApplicationSingle {
 public:
  ApplicationScenegraphClustered(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  ~ApplicationScenegraphClustered();
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
  void createTextureSamplers();

  void updateLightGrid();
  void createLights();
  void createUniformBuffers();

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

  // interaction
  void startTargetNavigation();
  void navigateToTarget();
  void manipulate();
  void endTargetManipulation();

  // path to the resource folders
  RenderPass m_render_pass;
  FrameBuffer m_framebuffer;
  Geometry m_model;
  ApplicationInstance m_instance;
  ModelLoader m_model_loader;
  Renderer m_renderer;
  Scenegraph m_graph;
  std::map<std::string, ModelNode> m_nodes;

  ComputePipeline m_pipeline_compute;
  Sampler m_volumeSampler;
  // required for light culling
  glm::uvec3 m_lightGridSize;
  glm::uvec2 m_tileSize;
  std::array<glm::vec4, 4> m_nearFrustumCornersClipSpace;
  // interaction
  bool m_selection_phase_flag;
  bool m_target_navi_phase_flag;
  bool m_navi_phase_flag;
  bool m_fly_phase_flag;
  bool m_manipulation_phase_flag;
  double m_target_navi_start;
  double m_target_navi_duration;
  // Node* m_curr_hit;
  Hit m_curr_hit;
  Navigation m_navigator;
  glm::vec3 m_cam_old_pos;
  glm::vec3 m_cam_new_pos;
  float m_frame_time;
  glm::vec3 m_last_translation;
  glm::fvec2 m_last_rotation;
  glm::fmat4 m_offset;
};

#endif