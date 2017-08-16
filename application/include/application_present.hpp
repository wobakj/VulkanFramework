#ifndef APPLICATION_COMPUTE_HPP
#define APPLICATION_COMPUTE_HPP

#include <vulkan/vulkan.hpp>

#include "app/application_win.hpp"
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

class ApplicationPresent : public ApplicationSingle<ApplicationWin> {
 public:
  ApplicationPresent(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse);
  ~ApplicationPresent();

  static cmdline::parser getParser(); 

 private:
  void logic() override final;
  void recordDrawBuffer(FrameResource& res) override;
  
  void createUniformBuffers();

  FrameResource createFrameResource() override;

  void createFrustra();
  void receiveData();
  // send out whether workers should stop
  void onFrameEnd() override final;
  void onResize(std::size_t width, std::size_t height) override;

  Memory m_memory_image;
  RenderPass m_render_pass;
  FrameBuffer m_framebuffer;
  ComputePipeline m_pipeline_compute;
  Sampler m_sampler;
  uint8_t* m_ptr_buff_transfer;
  std::vector<glm::fmat4> m_frustra;
  std::vector<vk::BufferImageCopy> m_copy_regions;
  glm::uvec2 m_frustum_cells;
};

#endif