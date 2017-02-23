#ifndef PIPELINE_INFO_HPP
#define PIPELINE_INFO_HPP

#include <vulkan/vulkan.hpp>
#include <vector>

class Shader;

class PipelineInfo {
 public:
  PipelineInfo();

  void setShader(Shader const& shader);

  void setVertexInput(vk::PipelineVertexInputStateCreateInfo const& input);

  void setTopology(vk::PrimitiveTopology const& topo);

  void setResolution(vk::Extent2D const& res);

  void setDepthStencil(vk::PipelineDepthStencilStateCreateInfo const& ds);

  void setRasterizer(vk::PipelineRasterizationStateCreateInfo const& raster);

  void setAttachmentBlending(vk::PipelineColorBlendAttachmentState const& attachment, uint32_t i);

  void setPass(vk::RenderPass const& pass, uint32_t subpass);
  
  void setRoot(vk::Pipeline const& pass);

  operator vk::GraphicsPipelineCreateInfo const&() const;

 private:
  vk::GraphicsPipelineCreateInfo info;
  vk::PipelineVertexInputStateCreateInfo info_vert;
  vk::PipelineInputAssemblyStateCreateInfo info_assembly;
  vk::PipelineViewportStateCreateInfo info_viewports;
  vk::PipelineMultisampleStateCreateInfo info_ms;
  vk::PipelineDepthStencilStateCreateInfo info_ds;
  vk::PipelineRasterizationStateCreateInfo info_raster;
  vk::PipelineColorBlendStateCreateInfo info_blending;

  std::vector<vk::PipelineColorBlendAttachmentState> attachment_blendings;

  vk::Viewport info_viewport;
  vk::Rect2D info_scissor;
};

#endif