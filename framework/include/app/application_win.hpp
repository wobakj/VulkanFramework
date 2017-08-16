#ifndef APPLICATION_WIN_HPP
#define APPLICATION_WIN_HPP

#include "app/application.hpp"
#include "wrap/swap_chain.hpp"
#include "statistics.hpp"
#include "wrap/conversions.hpp"

#include "camera.hpp"

#include "cmdline.h"

class FrameResource;
class Surface;

class ApplicationWin : public Application {
 public:
  // allocate and initialize objects
  ApplicationWin(std::string const& resource_path, Device& device, Surface const& surf, uint32_t image_count, cmdline::parser const& cmd_parse);
  // free resources
  virtual ~ApplicationWin();

  // react to key input
  void keyCallbackSelf(int key, int scancode, int action, int mods);
  virtual void keyCallback(int key, int scancode, int action, int mods) {};
  void resize(std::size_t width, std::size_t height) override;
  static cmdline::parser getParser(); 

  bool shouldClose() const override;

 protected:
  virtual FrameResource createFrameResource() override;
  virtual void acquireImage(FrameResource& res) final;
  virtual void presentFrame(FrameResource& res) final;
  virtual void presentFrame(FrameResource& res, vk::Queue const&) final;
  void onFrameBegin() override;

  virtual SubmitInfo createDrawSubmitInfo(FrameResource const& res) const override;

  glm::fmat4 const& matrixView() const;
  glm::fmat4 const& matrixFrustum() const;
  // container for the shader programs
  Camera m_camera;
  SwapChain m_swap_chain;
  Statistics m_statistics;
  Surface const* m_surface;

 private:
  // create chain
  void createSwapChain(Surface const& surf, cmdline::parser const& cmd_parse, uint32_t img_count);
};

#endif