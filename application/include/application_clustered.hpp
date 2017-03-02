#ifndef APPLICATION_CLUSTERED_HPP
#define APPLICATION_CLUSTERED_HPP

#include "app/application_single.hpp"

#include "deleter.hpp"
#include "light_grid.hpp"
#include "frame_resource.hpp"
#include "geometry.hpp"
#include "wrap/render_pass.hpp"
#include "wrap/frame_buffer.hpp"
#include "wrap/pipeline_info.hpp"
#include "wrap/pipeline.hpp"

#include <vulkan/vulkan.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

#include <thread>

class ApplicationClustered : public ApplicationSingle {
 public:
  ApplicationClustered(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow*, cmdline::parser const& cmd_parse);
  ~ApplicationClustered();
  static cmdline::parser getParser(); 
  static const uint32_t imageCount;
  
 private:
  void logic() override;
  void recordDrawBuffer(FrameResource& res) override;
  FrameResource createFrameResource() override;
  void updateResourceCommandBuffers(FrameResource& res) override;
  void updatePipelines() override;
  
  void createLights();
  void loadModel();
  void createUniformBuffers();
  void createVertexBuffer();
  void createTextureImages();
  void createTextureSamplers();

  float zFromFrustumSlice(unsigned int slice) const;
  glm::vec3 froxelCorner(unsigned int tileX,
                         unsigned int tileY,
                         float z,
                         std::vector<glm::vec3> const& frustumCorners) const;
  float pointFroxelDistance(glm::vec3 const& froxelFrontBottomLeft,
                            glm::vec3 const& froxelFrontTopRight,
                            glm::vec3 const& froxelBackBottomLeft,
                            glm::vec3 const& froxelBackTopRight,
                            glm::vec3 const& froxelCenter,
                            glm::vec3 const& planeNormal,
                            glm::vec3 const& point) const;
  void updateLightVolume();

  void updateView() override;

  void createFramebuffers() override;
  void createRenderPasses() override;
  void createPipelines() override;
  void createDescriptorPools() override;
  void createFramebufferAttachments() override;
  // handle key input
  void keyCallback(int key, int scancode, int action, int mods) override;

  // path to the resource folders
  RenderPass m_render_pass;
  FrameBuffer m_framebuffer;
  Geometry m_model;
  Deleter<VkDescriptorPool> m_descriptorPool;
  Deleter<VkDescriptorPool> m_descriptorPool_2;
  Deleter<VkSampler> m_textureSampler;
  Deleter<VkSampler> m_volumeSampler;
  std::thread m_thread_load;

  LightGrid m_light_grid;
  std::vector<uint32_t> m_data_light_volume;
};

#endif