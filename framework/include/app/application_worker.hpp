#ifndef APPLICATION_WORKER_HPP
#define APPLICATION_WORKER_HPP

#include "app/application.hpp"
#include "wrap/swap_chain.hpp"
#include "wrap/image_res.hpp"
#include "wrap/memory.hpp"
#include "statistics.hpp"
#include "wrap/conversions.hpp"

#include "camera.hpp"

#include "cmdline.h"

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
  void resize(std::size_t width, std::size_t height) override;
  static cmdline::parser getParser(); 

  bool shouldClose() const override;

 protected:
  virtual FrameResource createFrameResource() override;
  void createImages(uint32_t image_count);
  void createTransferBuffer();
  void acquireImage(FrameResource& res);
  virtual void presentFrame(FrameResource& res);
  void logic() override;

  void pushImageToDraw(uint32_t frame);
  uint32_t pullImageToDraw();
  // container for the shader programs
  // Camera m_camera;

  std::queue<uint32_t> m_queue_images;
  Statistics m_statistics;
  Memory m_memory_transfer;
  void* m_ptr_buff_transfer;
  std::vector<ImageRes> m_images_draw;
  bool m_should_close;
};

#endif