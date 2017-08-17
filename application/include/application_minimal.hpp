#ifndef APPLICATION_MINIMAL_HPP
#define APPLICATION_MINIMAL_HPP

#include <vulkan/vulkan.hpp>

#include "app/application_win.hpp"
#include "app/application_threaded.hpp"
#include "deleter.hpp"
#include "geometry.hpp"
#include "wrap/buffer.hpp"
#include "wrap/render_pass.hpp"
#include "wrap/memory.hpp"
#include "wrap/frame_buffer.hpp"
#include "wrap/fence.hpp"

#include <vector>
#include <atomic>

template<typename T>
class ApplicationMinimal : public T {
 public:
  ApplicationMinimal(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse);
  ~ApplicationMinimal();

  static cmdline::parser getParser(); 

 private:
  void recordDrawBuffer(FrameResource& res) override;
  void updateResourceCommandBuffers(FrameResource& res) override;
  void updatePipelines() override;
  
  void createFramebuffers() override;
  void createFramebufferAttachments() override;
  void createRenderPasses() override;
  void createPipelines() override;
  // handle key input
  // void keyCallback(int key, int scancode, int action, int mods) override;
  FrameResource createFrameResource() override;

  // path to the resource folders
  RenderPass m_render_pass;
  FrameBuffer m_framebuffer;
};

#include "application_minimal.inl"

#endif