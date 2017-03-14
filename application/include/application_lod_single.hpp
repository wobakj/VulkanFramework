#ifndef APPLICATION_LOD_HPP
#define APPLICATION_LOD_HPP

#include "app/application_single.hpp"

#include "geometry.hpp"
#include "geometry_lod.hpp"
#include "wrap/buffer.hpp"
#include "wrap/render_pass.hpp"
#include "wrap/memory.hpp"
#include "wrap/frame_buffer.hpp"
#include "wrap/fence.hpp"
#include "wrap/sampler.hpp"
#include "deleter.hpp"
#include "frame_resource.hpp"
#include "semaphore.hpp"
#include "averager.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <atomic>
#include <queue>
#include <thread>

class ApplicationLodSingle : public ApplicationSingle {
 public:
  ApplicationLodSingle(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  ~ApplicationLodSingle();
  static cmdline::parser getParser(); 
  static const uint32_t imageCount;

 private:
  void recordTransferBuffer(FrameResource& res);
  void recordDrawBuffer(FrameResource& res) override;
  void updateResourceCommandBuffers(FrameResource& res) override;
  void updateResourceDescriptors(FrameResource& resource) override;
  FrameResource createFrameResource() override;
  void updatePipelines() override;
  void updateDescriptors() override;
  
  void createLights();
  void loadModel();
  void createUniformBuffers();
  void createVertexBuffer(std::string const& lod_path, std::size_t cur_budged, std::size_t upload_budget);

  void createTextureImage();
  void createTextureSampler();

  void updateView() override;

  void createFramebuffers() override;
  void createRenderPasses() override;
  void createPipelines() override;
  void createDescriptorPools() override;
  void createFramebufferAttachments() override;
  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;
  void updateModel();

  // path to the resource folders
  RenderPass m_render_pass;
  FrameBuffer m_framebuffer;
  Geometry m_model_light;
  GeometryLod m_model_lod;
  Sampler m_sampler;

  bool m_setting_wire;
  bool m_setting_transparent;
  bool m_setting_shaded;
  bool m_setting_levels;
};

#endif