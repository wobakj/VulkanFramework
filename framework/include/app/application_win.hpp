#ifndef APPLICATION_WIN_HPP
#define APPLICATION_WIN_HPP

#include "app/application.hpp"
#include "wrap/swap_chain.hpp"

#include "camera.hpp"

#include "cmdline.h"

class FrameResource;

class ApplicationWin : public Application {
 public:
  // allocate and initialize objects
  ApplicationWin(std::string const& resource_path, Device& device, vk::SurfaceKHR const& surf, GLFWwindow*, cmdline::parser const& cmd_parse);
  // free resources
  virtual ~ApplicationWin(){};

  // react to key input
  inline virtual void keyCallback(int key, int scancode, int action, int mods) {};
  void resize(std::size_t width, std::size_t height) override;
  static cmdline::parser getParser(); 

  static const uint32_t imageCount;

 protected:
  // create chain
  void createSwapChain(vk::SurfaceKHR const& surf, cmdline::parser const& cmd_parse, uint32_t img_count);
  virtual FrameResource createFrameResource() override;
  
  void acquireImage(FrameResource& res);
  virtual void presentFrame(FrameResource& res);
  virtual void presentFrame(FrameResource& res, vk::Queue const&);
  // virtual void onResize(std::size_t width, std::size_t height);

  // container for the shader programs
  Camera m_camera;
  SwapChain m_swap_chain;
};

#endif