#ifndef LAUNCHER_VULKAN_HPP
#define LAUNCHER_VULKAN_HPP

#include <vulkan/vulkan.hpp>

#include "deleter.hpp"
#include "wrapper.hpp"
#include "device.hpp"
#include "model.hpp"
#include "image.hpp"
#include "instance.hpp"
#include "buffer.hpp"
#include "debug_reporter.hpp"
#include "swap_chain.hpp"
#include "camera.hpp"
#include "shader.hpp"
#include "render_pass.hpp"

#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <thread>
#include <map>

// forward declarations
class Application;
class GLFWwindow;

// struct QueueFamilyIndices {
//     int graphicsFamily = -1;
//     int presentFamily = -1;

//     bool isComplete() {
//       return graphicsFamily >= 0 && presentFamily >= 0;
//     }
// };

// struct SwapChainSupportDetails {
//     vk::SurfaceCapabilitiesKHR capabilities;
//     std::vector<vk::SurfaceFormatKHR> formats;
//     std::vector<vk::PresentModeKHR> presentModes;
// };
uint32_t findMemoryType(vk::PhysicalDevice const& device, uint32_t typeFilter, vk::MemoryPropertyFlags const& properties);

class LauncherVulkan {
 public:
  LauncherVulkan(int argc, char* argv[]);
  // create window and set callbacks
  void initialize();
  // start main loop
  void mainLoop();

 private:
  void draw();
  void createSemaphores();
  void createUniformBuffers();
  void createCommandBuffers();
  void updateCommandBuffers();
  void createFramebuffers();
  void createRenderPass();
  void createGraphicsPipeline();
  void recreateSwapChain();
  void createVertexBuffer();
  void createSurface();
  void createInstance();
  void updateUniformBuffer();
  void createDescriptorPool();
  void createTextureImage();
  void createTextureSampler();
  void createDepthResource();
  void loadModel();
  void createPrimaryCommandBuffer(int index_fb);

  void transitionImageLayout(vk::Image image, vk::ImageLayout const& oldLayout, vk::ImageLayout const& newLayout);
  
  // update viewport and field of view
  void update_projection(GLFWwindow* window, int width, int height);
  // load shader programs and update uniform locations
  void update_shader_programs(bool throwing);
  // handle key input
  void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
  // calculate fps and show in window title
  void show_fps();
  // free resources
  void quit(int status);

  // vertical field of view of camera
  const float m_camera_fov;

  // initial window dimensions
  unsigned m_window_width;
  unsigned m_window_height;
  // the rendering window
  GLFWwindow* m_window;

  // variables for fps computation
  double m_last_second_time;
  unsigned m_frames_per_second;

  // path to the resource folders
  std::string m_resource_path;
  const std::vector<const char*> m_validation_layers;
  Instance m_instance;
  DebugReporter m_debug_report;
  Deleter<VkSurfaceKHR> m_surface;
  RenderPass m_render_pass;
  Deleter<VkPipeline> m_pipeline;
  Deleter<VkPipeline> m_pipeline_2;
  Deleter<VkFramebuffer> m_framebuffer;
  std::map<std::string, vk::CommandBuffer> m_command_buffers;
  Deleter<VkSemaphore> m_sema_image_ready;
  Deleter<VkSemaphore> m_sema_render_done;
  SwapChain m_swap_chain;
  Device m_device;
  Model m_model;
  Model m_model_2;
  Buffer m_buffer_uniform; 
  Buffer m_buffer_uniform_stage; 
  Deleter<VkDescriptorPool> m_descriptorPool;
  Deleter<VkDescriptorPool> m_descriptorPool_2;
  vk::DescriptorSet m_descriptorSet;
  vk::DescriptorSet m_descriptorSet_2;
  Deleter<VkSampler> m_textureSampler;
  Deleter<VkFence> m_fence_draw;
  Deleter<VkFence> m_fence_command;
  Image m_image_depth;
  Image m_image_color;
  Image m_image_color_2;
  Image m_image;
  std::thread m_thread_load;
  std::atomic<bool> m_model_dirty;
  Camera m_camera;
  std::map<std::string, Shader> m_shaders;
  // Application* m_application;
};

#endif