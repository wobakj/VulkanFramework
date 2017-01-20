#ifndef APPLICATION_THREADED_MINIMAL_HPP
#define APPLICATION_THREADED_MINIMAL_HPP

#include <vulkan/vulkan.hpp>

#include "application_threaded.hpp"
#include "deleter.hpp"
#include "model.hpp"
#include "buffer.hpp"
#include "render_pass.hpp"
#include "memory.hpp"
#include "frame_buffer.hpp"
#include "fence.hpp"

#include <vector>
#include <atomic>

class ApplicationThreadedMin : public ApplicationThreaded {
 public:
  ApplicationThreadedMin(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  ~ApplicationThreadedMin();
  static const uint32_t imageCount;

 private:
  void recordDrawBuffer(FrameResource& res) override;
  void updateCommandBuffers(FrameResource& res) override;
  
  void createFramebuffers() override;
  void createFramebufferAttachments() override;
  void createRenderPasses() override;
  void createMemoryPools() override;
  void createPipelines() override;
  // handle key input
  // void keyCallback(int key, int scancode, int action, int mods) override;
  FrameResource createFrameResource() override;

  // path to the resource folders
  RenderPass m_render_pass;
  Deleter<VkPipeline> m_pipeline;
  FrameBuffer m_framebuffer;
};

#endif