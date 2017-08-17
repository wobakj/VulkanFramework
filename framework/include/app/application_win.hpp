#ifndef APPLICATION_WIN_HPP
#define APPLICATION_WIN_HPP

#include "app/application.hpp"
#include "wrap/swap_chain.hpp"

#include "camera.hpp"

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
  void acquireImage(FrameResource& res);
  void presentFrame(FrameResource& res);
  void presentFrame(FrameResource& res, vk::Queue const&);

  glm::fmat4 const& matrixView() const;
  glm::fmat4 const& matrixFrustum() const;
  
  virtual void onFrameBegin() override final;

  virtual FrameResource createFrameResource() override;
  virtual SubmitInfo createDrawSubmitInfo(FrameResource const& res) const override;

  Camera m_camera;
  SwapChain m_swap_chain;
  Surface const* m_surface;

 private:
  // create chain
  void createSwapChain(Surface const& surf, cmdline::parser const& cmd_parse, uint32_t img_count);
};

#endif