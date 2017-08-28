#ifndef APPLICATION_PRESENT_HPP
#define APPLICATION_PRESENT_HPP

#include "wrap/render_pass.hpp"
#include "wrap/frame_buffer.hpp"
#include "wrap/pipeline.hpp"
#include "allocator_static.hpp"

#include <vector>

template<typename T>
class ApplicationPresent : public T {
 public:
  ApplicationPresent(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse);
  ~ApplicationPresent();

  static cmdline::parser getParser(); 

 private:
  virtual void logic() override final;
  virtual void recordDrawBuffer(FrameResource& res) override;
  virtual void updateFrameResources() override;
  
  void createReceiveBuffer();
  virtual FrameResource createFrameResource() override;

  void createFrustra();
  void receiveData(FrameResource& res);
  // send out whether workers should stop
  virtual void onFrameEnd() override final;
  virtual void onResize() override;

  void pushForCopy(uint32_t frame);
  uint32_t pullForCopy();

  StaticAllocator m_allocator;
  RenderPass m_render_pass;
  FrameBuffer m_framebuffer;
  ComputePipeline m_pipeline_compute;
  uint8_t* m_ptr_buff_transfer;
  std::vector<glm::fmat4> m_frustra;
  std::vector<std::vector<vk::BufferImageCopy>> m_copy_regions;
  glm::uvec2 m_frustum_cells;
};

#include "application_present.inl"

#endif