#ifndef LAUNCHER_VULKAN_HPP
#define LAUNCHER_VULKAN_HPP

#include <vulkan/vulkan.hpp>

#include "deleter.hpp"
#include "wrapper.hpp"
#include "instance.hpp"
#include "debug_reporter.hpp"

#include <string>
#include <vector>

// forward declarations
class Application;
class GLFWwindow;

struct QueueFamilyIndices {
    int graphicsFamily = -1;
    int presentFamily = -1;

    bool isComplete() {
      return graphicsFamily >= 0 && presentFamily >= 0;
    }
};

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

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
  void createCommandBuffers();
  void createCommandPool();
  void createFramebuffers();
  void createRenderPass();
  void createShaderModule(const std::vector<char>& code, Deleter<VkShaderModule>& shaderModule);
  void createGraphicsPipeline();
  void createImageViews();
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
  void createSwapChain();
  SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device);
  bool isDeviceSuitable(vk::PhysicalDevice device);
  QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);
  void createSurface();
  void createLogicalDevice();
  void pickPhysicalDevice(); 
  void createInstance();
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
  const unsigned m_window_width;
  const unsigned m_window_height;
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
  vk::PhysicalDevice m_physical_device;
  vk::Queue m_queue_graphics;
  vk::Queue m_queue_present;
  Deleter<VkSurfaceKHR> m_surface;
  Deleter<VkSwapchainKHR> m_swap_chain;
  std::vector<vk::Image> m_images_swap;
  vk::Format m_format_swap;
  vk::Extent2D m_extent_swap;
  std::vector<Deleter<VkImageView>> m_views_swap;
  Deleter<VkPipelineLayout> m_pipeline_layout;
  Deleter<VkRenderPass> m_render_pass;
  Deleter<VkPipeline> m_pipeline;
  std::vector<Deleter<VkFramebuffer>> m_framebuffers;
  Deleter<VkCommandPool> m_command_pool;
  std::vector<vk::CommandBuffer> m_command_buffers;
  
  Deleter<VkSemaphore> m_sema_image_ready;
  Deleter<VkSemaphore> m_sema_render_done;
  Wrapper<vk::Device> m_device;
  // Deleter<VkDevice> m_device;

  // Application* m_application;
};

#endif