#include "pipeline_info.hpp"

#include "shader.hpp"
#include "model.hpp"

PipelineInfo::PipelineInfo()
 :info{}
 ,info_vert{}
 ,info_assembly{}
 ,info_viewports{}
 ,info_ms{}
 ,info_ds{}
 ,info_raster{}
 ,info_blending{}
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

  info_ds.depthTestEnable = VK_TRUE;
  info_ds.depthWriteEnable = VK_TRUE;
  info_ds.depthCompareOp = vk::CompareOp::eLess;
  // VkDynamicState dynamicStates[] = {
  //   VK_DYNAMIC_STATE_VIEWPORT,
  //   VK_DYNAMIC_STATE_LINE_WIDTH
  // };

  // VkPipelineDynamicStateCreateInfo dynamicState = {};
  // dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  // dynamicState.dynamicStateCount = 2;
  // dynamicState.pDynamicStates = dynamicStates;
  vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment.blendEnable = VK_FALSE;
  std::vector<vk::PipelineColorBlendAttachmentState> states{colorBlendAttachment, colorBlendAttachment, colorBlendAttachment};

  info.pDynamicState = nullptr; // Optional

  info.flags = vk::PipelineCreateFlagBits::eAllowDerivatives;
  info.basePipelineIndex = -1; // Optional
} 

void PipelineInfo::setShader(Shader const& shader) {
  auto info_start = shader.startPipelineInfo();

  info.stageCount = info_start.stageCount;
  info.pStages = info_start.pStages;
  info.layout = info_start.layout;
}

void PipelineInfo::setVertexInput(vk::PipelineVertexInputStateCreateInfo const& input) {
  info_vert = input;
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

void PipelineInfo::setRoot(vk::Pipeline const& root) {
  if (root) {
    info.flags |= vk::PipelineCreateFlagBits::eDerivative;
    // insert previously created pipeline here to derive this one from
    info.basePipelineHandle = root;
  }
}

PipelineInfo::operator vk::GraphicsPipelineCreateInfo const&() const {
  return info;
}