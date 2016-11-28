#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "shader.hpp"
#include "image.hpp"
#include "buffer.hpp"
#include "camera.hpp"
#include "device.hpp"
#include "swap_chain.hpp"

#include <map>
#include <mutex>

class FrameResource;

class Application {
 public:
  // allocate and initialize objects
  Application(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*);
  // free resources
  virtual ~Application(){};

  // react to key input
  inline virtual void keyCallback(int key, int scancode, int action, int mods) {};
  // 
  void updateShaderPrograms();
  // draw all objects
  virtual void update();
  void resize(std::size_t with, std::size_t height);

  void blockSwapChain();
  void unblockSwapChain();

 protected:
  virtual void render() = 0;
  virtual void updateView() = 0;
  virtual void recreatePipeline() = 0;
  virtual void resize() = 0;
  
  void acquireImage(FrameResource& res);
  void present(FrameResource& res);
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
  std::mutex m_mutex_swapchain;

 private:
};

#endif