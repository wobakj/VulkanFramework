#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "camera.hpp"
#include "device.hpp"
#include "swap_chain.hpp"

#include <map>

class Image;
class Shader;
class Buffer;

class Application {
 public:
  // allocate and initialize objects
  Application(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*);
  // free
  virtual ~Application();

  // react to key input
  inline virtual void keyCallback(int key, int scancode, int action, int mods) {};
  // 
  void updateShaderPrograms();
  // draw all objects
  virtual void update();
  void resize(std::size_t with, std::size_t height);


  vk::Semaphore const& semaphoreAcquire();
  vk::Semaphore const& semaphoreDraw();
  vk::Fence const& fenceDraw();

 protected:
  virtual void render() = 0;
  virtual void updateView() = 0;
  virtual void recreatePipeline() = 0;
  virtual void resize() = 0;
  
  void initialize();

  uint32_t acquireImage();
  void submitDraw(vk::CommandBuffer const& buffer);
  void present(uint32_t index_image);

  std::string m_resource_path; 

  // container for the shader programs
  Camera m_camera;
  Device& m_device;
  SwapChain const& m_swap_chain;

  std::map<std::string, vk::CommandBuffer> m_command_buffers;
  std::map<std::string, vk::DescriptorSet> m_descriptor_sets;
  std::map<std::string, vk::Semaphore> m_semaphores;
  std::map<std::string, vk::Fence> m_fences;
  std::map<std::string, Shader> m_shaders;
  std::map<std::string, Image> m_images;
  std::map<std::string, Buffer> m_buffers;

 private:
};

#endif