#ifndef APPLICATION_VULKAN_HPP
#define APPLICATION_VULKAN_HPP

#include <vulkan/vulkan.hpp>

#include "application.hpp"
#include "deleter.hpp"
#include "wrapper.hpp"
#include "device.hpp"
#include "model.hpp"
#include "image.hpp"
#include "instance.hpp"
#include "buffer.hpp"
#include "swap_chain.hpp"
#include "camera.hpp"
#include "shader.hpp"
#include "render_pass.hpp"
#include "memory.hpp"
#include "frame_buffer.hpp"

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

class ApplicationVulkan : public Application {
 public:
  ApplicationVulkan(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*);

 private:
  void render() override;
  void createLights();
  void updateLights();
  void createSemaphores();
  void createUniformBuffers();
  void createCommandBuffers();
  void updateCommandBuffers();
  void createFramebuffers();
  void createRenderPass();
  void createMemoryPools();
  void createGraphicsPipeline();
  void resize() override;
  void recreatePipeline() override;
  void createVertexBuffer();
  void updateView() override;
  void createDescriptorPool();
  void createTextureImage();
  void createTextureSampler();
  void createDepthResource();
  void loadModel();
  void createPrimaryCommandBuffer(int index_fb);
  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;

  // path to the resource folders
  RenderPass m_render_pass;
  Deleter<VkPipeline> m_pipeline;
  Deleter<VkPipeline> m_pipeline_2;
  FrameBuffer m_framebuffer;
  std::map<std::string, vk::CommandBuffer> m_command_buffers;
  Deleter<VkSemaphore> m_sema_render_done;
  Model m_model;
  Model m_model_2;
  Buffer m_buffer_light; 
  Buffer m_buffer_light_stage; 
  Buffer m_buffer_uniform; 
  Buffer m_buffer_uniform_stage; 
  Deleter<VkDescriptorPool> m_descriptorPool;
  Deleter<VkDescriptorPool> m_descriptorPool_2;
  vk::DescriptorSet m_descriptorSet;
  vk::DescriptorSet m_descriptorSet_3;
  vk::DescriptorSet m_descriptorSet_2;
  std::map<std::string, vk::DescriptorSet> m_descriptor_sets;
  Deleter<VkSampler> m_textureSampler;
  Deleter<VkFence> m_fence_draw;
  Deleter<VkFence> m_fence_command;
  Image m_image_depth;
  Image m_image_color;
  Image m_image_pos;
  Image m_image_normal;
  Image m_image_color_2;
  Image m_image;
  std::thread m_thread_load;
  std::atomic<bool> m_model_dirty;
  std::map<std::string, Shader> m_shaders;
  // Application* m_application;
  bool m_sphere;
};

#endif