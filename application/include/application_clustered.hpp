#ifndef APPLICATION_CLUSTERED_HPP
#define APPLICATION_CLUSTERED_HPP

#include "geometry.hpp"
#include "wrap/render_pass.hpp"
#include "wrap/frame_buffer.hpp"
#include "wrap/pipeline_info.hpp"
#include "wrap/pipeline.hpp"
#include "wrap/sampler.hpp"

#include <glm/gtc/type_precision.hpp>

class Surface;
class Device;
class FrameResource;

namespace cmdline {
  class parser;
}

template<typename T>
class ApplicationClustered : public T  {
 public:
  ApplicationClustered(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse);
  ~ApplicationClustered();

  static cmdline::parser getParser(); 
  
 private:
  void logic() override;
  void recordDrawBuffer(FrameResource& res) override;
  FrameResource createFrameResource() override;
  void updateResourceCommandBuffers(FrameResource& res) override;
  void updatePipelines() override;
  void updateDescriptors() override;
  
  void createLights();
  void loadModel();
  void createUniformBuffers();
  void createVertexBuffer();
  void createTextureImages();
  void createTextureSamplers();

  void updateLightGrid();

  void updateView();

  void createFramebuffers() override;
  void createRenderPasses() override;
  void createPipelines() override;
  void createDescriptorPools() override;
  void createFramebufferAttachments() override;

  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;

  // path to the resource folders
  RenderPass m_render_pass;
  FrameBuffer m_framebuffer;
  ComputePipeline m_pipeline_compute;
  Geometry m_model;
  Sampler m_sampler;
  Sampler m_volumeSampler;

  // required for light culling
  glm::uvec3 m_lightGridSize;
  glm::uvec2 m_tileSize;
  std::array<glm::vec4, 4> m_nearFrustumCornersClipSpace;
};

#include "application_clustered.inl"

#endif