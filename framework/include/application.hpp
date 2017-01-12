#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "shader.hpp"
#include "image.hpp"
#include "buffer.hpp"
#include "buffer_view.hpp"
#include "camera.hpp"
#include "device.hpp"
#include "swap_chain.hpp"

#include <map>
#include <mutex>

class FrameResource;

class Application {
 public:
  // allocate and initialize objects
  Application(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, std::vector<std::string> const& args);
  // free resources
  virtual ~Application(){};

  // react to key input
  inline virtual void keyCallback(int key, int scancode, int action, int mods) {};
  // 
  void updateShaderPrograms();
  // draw all objects
  void frame();
  void resize(std::size_t with, std::size_t height);

  // render remaining recorded frames before pipeline rebuild
  // required for multithreaded rendering
  virtual void emptyDrawQueue() {};

 protected:
  virtual void update() {};
  virtual void render() = 0;
  virtual void updateView() = 0;
  virtual void recreatePipeline() = 0;
  virtual void resize() = 0;
  
  void acquireImage(FrameResource& res);
  virtual void present(FrameResource& res);
  virtual void present(FrameResource& res, vk::Queue const&);
  void submitDraw(FrameResource& res);
  virtual void recordDrawBuffer(FrameResource& res) = 0;

  std::string m_resource_path; 

  // container for the shader programs
  Camera m_camera;
  Device& m_device;
  SwapChain const& m_swap_chain;

  std::map<std::string, vk::DescriptorSet> m_descriptor_sets;
  std::map<std::string, Shader> m_shaders;
  std::map<std::string, Image> m_images;
  std::map<std::string, Buffer> m_buffers;
  std::map<std::string, BufferView> m_buffer_views;

 private:
};

#endif