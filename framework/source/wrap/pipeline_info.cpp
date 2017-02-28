#include "wrap/pipeline_info.hpp"

#include "wrap/shader.hpp"
#include "geometry.hpp"
#include "geometry_lod.hpp"

PipelineInfo::PipelineInfo()
 :info{}
 ,info_vert{}
 ,info_assembly{}
 ,info_viewports{}
 ,info_ms{}
 ,info_ds{}
 ,info_raster{}
 ,info_blending{}
 ,info_dynamic{}
 ,info_viewport{}
 ,info_scissor{}
{
  info.pVertexInputState = &info_vert;
  info.pInputAssemblyState = &info_assembly;
  info.pViewportState = &info_viewports;  
  info.pMultisampleState = &info_ms;
  info.pRasterizationState = &info_raster;
  info.pDepthStencilState = &info_ds;
  info.pColorBlendState = &info_blending;

  info_viewport.minDepth = 0.0f;
  info_viewport.maxDepth = 1.0f;

  info_viewports.viewportCount = 1;
  info_viewports.pViewports = &info_viewport;
  info_viewports.scissorCount = 1;
  info_viewports.pScissors = &info_scissor;

  info_raster.lineWidth = 1.0f;
  info_raster.cullMode = vk::CullModeFlagBits::eBack;

  // VkDynamicState dynamicStates[] = {
  //   VK_DYNAMIC_STATE_VIEWPORT,
  //   VK_DYNAMIC_STATE_LINE_WIDTH
  // };

  info.pDynamicState = nullptr; // Optional

  info.flags = vk::PipelineCreateFlagBits::eAllowDerivatives;
  info.basePipelineIndex = -1; // Optional
} 

PipelineInfo::PipelineInfo(PipelineInfo const& rhs)
 :PipelineInfo{}
{
  info.layout = rhs.info.layout;
  setShaderStages(rhs.info_stages);

  setVertexBindings(rhs.info_bindings);
  setVertexAttributes(rhs.info_attributes);

  setTopology(rhs.info_assembly.topology);
  setResolution(rhs.info_scissor.extent);
  setDepthStencil(rhs.info_ds);
  setRasterizer(rhs.info_raster);

  for (uint32_t i = 0; i < rhs.attachment_blendings.size(); ++i) {
    setAttachmentBlending(rhs.attachment_blendings[i], i);
  }

  for (auto const& state : rhs.info_dynamics) {
    addDynamic(state);
  }

  setPass(rhs.info.renderPass, rhs.info.subpass);

  if (rhs.info.basePipelineHandle) {
    setRoot(rhs.info.basePipelineHandle);
  }
}

void PipelineInfo::setShader(Shader const& shader) {
  info.layout = shader.get();
  setShaderStages(shader.shaderStages());
}

void PipelineInfo::setVertexInput(Geometry const& model) {
  setVertexBindings(model.bindInfos());
  setVertexAttributes(model.attributeInfos());
}

void PipelineInfo::setVertexInput(GeometryLod const& model) {
  setVertexBindings(model.bindInfos());
  setVertexAttributes(model.attributeInfos());
}

void PipelineInfo::setShaderStages(std::vector<vk::PipelineShaderStageCreateInfo> const& stages) {
  info_stages = stages;
  info.stageCount = uint32_t(info_stages.size());
  info.pStages = info_stages.data();
}

void PipelineInfo::setVertexBindings(std::vector<vk::VertexInputBindingDescription> const& bindings) {
  info_bindings = bindings;
  info_vert.vertexBindingDescriptionCount = std::uint32_t(info_bindings.size());
  info_vert.pVertexBindingDescriptions = info_bindings.data();
}

void PipelineInfo::setVertexAttributes(std::vector<vk::VertexInputAttributeDescription> const& attributes) {
  info_attributes = attributes;
  info_vert.vertexAttributeDescriptionCount = std::uint32_t(info_attributes.size());
  info_vert.pVertexAttributeDescriptions = info_attributes.data();
}

void PipelineInfo::setTopology(vk::PrimitiveTopology const& topo) {
  info_assembly.topology = topo;
}

void PipelineInfo::setResolution(vk::Extent2D const& res) {
  info_viewport.width = float(res.width);
  info_viewport.height = float(res.height); 

  info_scissor.extent = res;   
}

void PipelineInfo::setDepthStencil(vk::PipelineDepthStencilStateCreateInfo const& ds) {
  info_ds = ds;
}

void PipelineInfo::setRasterizer(vk::PipelineRasterizationStateCreateInfo const& raster) {
  info_raster = raster;
}

void PipelineInfo::setAttachmentBlending(vk::PipelineColorBlendAttachmentState const& attachment, uint32_t i) {
  if (i >= attachment_blendings.size()) {
    attachment_blendings.resize(i + 1);
  }

  attachment_blendings[i] = attachment;

  info_blending.attachmentCount = uint32_t(attachment_blendings.size());
  info_blending.pAttachments = attachment_blendings.data();
}

void PipelineInfo::setPass(vk::RenderPass const& pass, uint32_t subpass) {
  info.renderPass = pass;
  info.subpass = subpass;
}

void PipelineInfo::addDynamic(vk::DynamicState const& state) {
  info.pDynamicState = &info_dynamic;
  
  info_dynamics.emplace_back(state);
  info_dynamic.dynamicStateCount = uint32_t(info_dynamics.size());
  info_dynamic.pDynamicStates = info_dynamics.data();
}

void PipelineInfo::setRoot(vk::Pipeline const& root) {
  info.flags |= vk::PipelineCreateFlagBits::eDerivative;
  // insert previously created pipeline here to derive this one from
  info.basePipelineHandle = root;
}

PipelineInfo::operator vk::GraphicsPipelineCreateInfo const&() const {
  return info;
}

vk::PipelineLayout const& PipelineInfo::layout() const {
  return info.layout;
}

vk::PipelineRasterizationStateCreateInfo const& PipelineInfo::rasterizer() const {
  return info_raster;
}

vk::PipelineDepthStencilStateCreateInfo const& PipelineInfo::depthStencil() const {
  return info_ds;
}