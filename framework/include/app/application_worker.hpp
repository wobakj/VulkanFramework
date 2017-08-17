#ifndef APPLICATION_WORKER_HPP
#define APPLICATION_WORKER_HPP

#include "app/application.hpp"
#include "wrap/image_res.hpp"
#include "wrap/memory.hpp"

#include <queue>

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
  virtual void resize(std::size_t width, std::size_t height) override final;
  static cmdline::parser getParser(); 

  bool shouldClose() const override;

 protected:
  void acquireImage(FrameResource& res);
  void presentFrame(FrameResource& res);

  glm::fmat4 const& matrixView() const;
  glm::fmat4 const& matrixFrustum() const;

  virtual void onFrameEnd() override final;
  virtual void onFrameBegin() override final;

 private:
  void createImages(uint32_t image_count);
  void createTransferBuffer();
  void pushImageToDraw(uint32_t frame);
  uint32_t pullImageToDraw();

 private:
  std::queue<uint32_t> m_queue_images;
  Memory m_memory_transfer;
  void* m_ptr_buff_transfer;
  std::vector<ImageRes> m_images_draw;
  bool m_should_close;
  glm::fmat4 m_mat_view;
  glm::fmat4 m_mat_frustum;
};

#endif