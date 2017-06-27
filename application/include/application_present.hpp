#ifndef APPLICATION_COMPUTE_HPP
#define APPLICATION_COMPUTE_HPP

#include <vulkan/vulkan.hpp>

#include "app/application_single.hpp"
#include "deleter.hpp"
#include "geometry.hpp"
#include "wrap/buffer.hpp"
#include "wrap/render_pass.hpp"
#include "wrap/memory.hpp"
#include "wrap/frame_buffer.hpp"
#include "wrap/fence.hpp"
#include "wrap/pipeline.hpp"
#include "wrap/sampler.hpp"

#include <vector>
#include <atomic>

class ApplicationPresent : public ApplicationSingle {
 public:
  ApplicationPresent(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse);
  ~ApplicationPresent();

  static cmdline::parser getParser(); 

 private:
  void recordDrawBuffer(FrameResource& res) override;
  void updateResourceCommandBuffers(FrameResource& res) override;
  void updatePipelines() override;
  void updateDescriptors() override;
  
  void createTextureImages();
  void createUniformBuffers();
  void updateUniformBuffers();

  void createFramebuffers() override {};
  void createFramebufferAttachments() override {};
  void createRenderPasses() override {};

  void createDescriptorPools() override;
  void createPipelines() override;
  // handle key input
  // void keyCallback(int key, int scancode, int action, int mods) override;
  FrameResource createFrameResource() override;

  // path to the resource folders
  RenderPass m_render_pass;
  FrameBuffer m_framebuffer;
  ComputePipeline m_pipeline_compute;
  Sampler m_sampler;
};

#endif