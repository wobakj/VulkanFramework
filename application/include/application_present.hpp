#ifndef APPLICATION_PRESENT_HPP
#define APPLICATION_PRESENT_HPP

#include "wrap/render_pass.hpp"
#include "wrap/frame_buffer.hpp"
#include "wrap/pipeline.hpp"
#include "wrap/sampler.hpp"

#include <vector>
#include <atomic>

template<typename T>
class ApplicationPresent : public T {
 public:
  ApplicationPresent(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse);
  ~ApplicationPresent();

  static cmdline::parser getParser(); 

 private:
  virtual void logic() override final;
  virtual void recordDrawBuffer(FrameResource& res) override;
  
  void createReceiveBuffer();

  void createFrustra();
  void receiveData();
  // send out whether workers should stop
  virtual void onFrameEnd() override final;
  virtual void onResize() override;

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

#include "application_present.inl"

#endif