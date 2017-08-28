#ifndef APPLICATION_WORKER_HPP
#define APPLICATION_WORKER_HPP

#include "app/application.hpp"
#include "wrap/image_res.hpp"

#include "allocator_static.hpp"

class FrameResource;
class Surface;

class ApplicationWorker : public Application {
 public:
  // allocate and initialize objects
  ApplicationWorker(std::string const& resource_path, Device& device, Surface const& surf, uint32_t image_count, cmdline::parser const& cmd_parse);
  // free resources
  virtual ~ApplicationWorker();

  // react to key input
  inline virtual void keyCallback(int key, int scancode, int action, int mods) {};
  static cmdline::parser getParser(); 

  bool shouldClose() const override;

 protected:
  void acquireImage(FrameResource& res);
  void presentFrame(FrameResource& res);
  virtual void onResize() override;
  virtual void updateFrameResources() override;

  glm::fmat4 const& matrixView() const;
  glm::fmat4 const& matrixFrustum() const;

  virtual void onFrameEnd() override final;
  virtual void onFrameBegin() override final;

  virtual void presentCommands(FrameResource& res, ImageView const& view, vk::ImageLayout const& layout) override;

 private:
  void createImages(uint32_t image_count);
  void createSendBuffer();
  void pushImageToDraw(uint32_t frame);
  uint32_t pullImageToDraw();

 private:
  StaticAllocator m_allocator;
  uint8_t* m_ptr_buff_transfer;
  std::vector<vk::BufferImageCopy> m_copy_regions;
  bool m_should_close;
  glm::fmat4 m_mat_view;
  glm::fmat4 m_mat_frustum;
};

#endif