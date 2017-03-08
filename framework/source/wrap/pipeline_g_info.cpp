#include "wrap/pipeline_g_info.hpp"

#include "wrap/shader.hpp"
#include "geometry.hpp"
#include "geometry_lod.hpp"

GraphicsPipelineInfo::GraphicsPipelineInfo()
 :PipelineInfo{}
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

  // info.flags = vk::PipelineCreateFlagBits::eAllowDerivatives;
  // info.basePipelineIndex = -1; // Optional
} 

GraphicsPipelineInfo::GraphicsPipelineInfo(GraphicsPipelineInfo const& rhs)
 :GraphicsPipelineInfo{}
{
  info.layout = rhs.info.layout;
  // copy spec infos before shader stages so pointer to them gets set correctly
  m_spec_infos = rhs.m_spec_infos;
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

void GraphicsPipelineInfo::setShader(Shader const& shader) {
  info.layout = shader.get();
  setShaderStages(shader.shaderStages());
}

void GraphicsPipelineInfo::setVertexInput(Geometry const& model) {
  setVertexBindings(model.bindInfos());
  setVertexAttributes(model.attributeInfos());
}

void GraphicsPipelineInfo::setVertexInput(GeometryLod const& model) {
  setVertexBindings(model.bindInfos());
  setVertexAttributes(model.attributeInfos());
}

void GraphicsPipelineInfo::setShaderStages(std::vector<vk::PipelineShaderStageCreateInfo> const& stages) {
  info_stages = stages;
  info.stageCount = uint32_t(info_stages.size());
  info.pStages = info_stages.data();
  for(auto& stage_info : info_stages) {
    auto iter = m_spec_infos.find(stage_info.stage);
    if (iter == m_spec_infos.end()) {
      m_spec_infos.emplace(stage_info.stage, SpecInfo{});
    }
    stage_info.pSpecializationInfo = &m_spec_infos.at(stage_info.stage).get();
  }
  // TODO: support for changing of featured stages on load
}

void GraphicsPipelineInfo::setVertexBindings(std::vector<vk::VertexInputBindingDescription> const& bindings) {
  info_bindings = bindings;
  info_vert.vertexBindingDescriptionCount = std::uint32_t(info_bindings.size());
  info_vert.pVertexBindingDescriptions = info_bindings.data();
}

void GraphicsPipelineInfo::setVertexAttributes(std::vector<vk::VertexInputAttributeDescription> const& attributes) {
  info_attributes = attributes;
  info_vert.vertexAttributeDescriptionCount = std::uint32_t(info_attributes.size());
  info_vert.pVertexAttributeDescriptions = info_attributes.data();
}

void GraphicsPipelineInfo::setTopology(vk::PrimitiveTopology const& topo) {
  info_assembly.topology = topo;
}

void GraphicsPipelineInfo::setResolution(vk::Extent2D const& res) {
  info_viewport.width = float(res.width);
  info_viewport.height = float(res.height); 

  info_scissor.extent = res;   
}

void GraphicsPipelineInfo::setDepthStencil(vk::PipelineDepthStencilStateCreateInfo const& ds) {
  info_ds = ds;
}

void GraphicsPipelineInfo::setRasterizer(vk::PipelineRasterizationStateCreateInfo const& raster) {
  info_raster = raster;
}

void GraphicsPipelineInfo::setAttachmentBlending(vk::PipelineColorBlendAttachmentState const& attachment, uint32_t i) {
  if (i >= attachment_blendings.size()) {
    attachment_blendings.resize(i + 1);
  }

  attachment_blendings[i] = attachment;

  info_blending.attachmentCount = uint32_t(attachment_blendings.size());
  info_blending.pAttachments = attachment_blendings.data();
}

void GraphicsPipelineInfo::setPass(vk::RenderPass const& pass, uint32_t subpass) {
  info.renderPass = pass;
  info.subpass = subpass;
}

void GraphicsPipelineInfo::addDynamic(vk::DynamicState const& state) {
  info.pDynamicState = &info_dynamic;
  
  info_dynamics.emplace_back(state);
  info_dynamic.dynamicStateCount = uint32_t(info_dynamics.size());
  info_dynamic.pDynamicStates = info_dynamics.data();
}

vk::PipelineRasterizationStateCreateInfo const& GraphicsPipelineInfo::rasterizer() const {
  return info_raster;
}

vk::PipelineDepthStencilStateCreateInfo const& GraphicsPipelineInfo::depthStencil() const {
  return info_ds;
}