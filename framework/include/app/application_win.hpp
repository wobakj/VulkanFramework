#ifndef APPLICATION_WIN_HPP
#define APPLICATION_WIN_HPP

#include "app/application.hpp"
#include "wrap/swap_chain.hpp"
#include "statistics.hpp"

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
  inline virtual void keyCallback(int key, int scancode, int action, int mods) {};
  void resize(std::size_t width, std::size_t height) override;
  static cmdline::parser getParser(); 

 protected:
  virtual FrameResource createFrameResource() override;
  void acquireImage(FrameResource& res);
  virtual void presentFrame(FrameResource& res);
  virtual void presentFrame(FrameResource& res, vk::Queue const&);
  void logic() override;
  // container for the shader programs
  Camera m_camera;
  SwapChain m_swap_chain;
  Statistics m_statistics;

 private:
  // create chain
  void createSwapChain(Surface const& surf, cmdline::parser const& cmd_parse, uint32_t img_count);
};

#endif