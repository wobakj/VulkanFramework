#ifndef PIPELINE_INFO_HPP
#define PIPELINE_INFO_HPP

#include <vulkan/vulkan.hpp>
#include <vector>

class Shader;
class Model;
class ModelLod;

class PipelineInfo {
 public:
  PipelineInfo();
  PipelineInfo(PipelineInfo const&);

  void setShader(Shader const& shader);

  void setVertexInput(Model const& model);
  void setVertexInput(ModelLod const& model);

  void setTopology(vk::PrimitiveTopology const& topo);

  void setResolution(vk::Extent2D const& res);

  void setDepthStencil(vk::PipelineDepthStencilStateCreateInfo const& ds);

  void setRasterizer(vk::PipelineRasterizationStateCreateInfo const& raster);

  void setAttachmentBlending(vk::PipelineColorBlendAttachmentState const& attachment, uint32_t i);

  void setPass(vk::RenderPass const& pass, uint32_t subpass);
  
  void setRoot(vk::Pipeline const& pass);

  void addDynamic(vk::DynamicState const& state);

  operator vk::GraphicsPipelineCreateInfo const&() const;

 private:
  void setVertexAttributes(std::vector<vk::VertexInputAttributeDescription> const& attributes);
  void setShaderStages(std::vector<vk::PipelineShaderStageCreateInfo> const& stages);
  void setVertexBindings(std::vector<vk::VertexInputBindingDescription> const& bindings);

  vk::GraphicsPipelineCreateInfo info;
  vk::PipelineVertexInputStateCreateInfo info_vert;
  vk::PipelineInputAssemblyStateCreateInfo info_assembly;
  vk::PipelineViewportStateCreateInfo info_viewports;
  vk::PipelineMultisampleStateCreateInfo info_ms;
  vk::PipelineDepthStencilStateCreateInfo info_ds;
  vk::PipelineRasterizationStateCreateInfo info_raster;
  vk::PipelineColorBlendStateCreateInfo info_blending;
  vk::PipelineDynamicStateCreateInfo info_dynamic;
  // secondary resources
  std::vector<vk::PipelineColorBlendAttachmentState> attachment_blendings;
  std::vector<vk::PipelineShaderStageCreateInfo> info_stages;
  // viewport
  vk::Viewport info_viewport;
  vk::Rect2D info_scissor;
  // vertex input
  std::vector<vk::VertexInputBindingDescription> info_bindings;
  std::vector<vk::VertexInputAttributeDescription> info_attributes;
  
  std::vector<vk::DynamicState> info_dynamics;
};

#endif