#ifndef PIPELINE_G_INFO_HPP
#define PIPELINE_G_INFO_HPP

#include "wrap/pipeline_info.hpp"
#include "wrap/vertex_info.hpp"

#include <vulkan/vulkan.hpp>
#include <vector>
#include <map>
#include <stdexcept>

class Geometry;
class GeometryLod;

class GraphicsPipelineInfo : public PipelineInfo<vk::GraphicsPipelineCreateInfo> {
 public:
  GraphicsPipelineInfo();
  GraphicsPipelineInfo(GraphicsPipelineInfo const&);
  GraphicsPipelineInfo(GraphicsPipelineInfo&&);
  GraphicsPipelineInfo& operator=(GraphicsPipelineInfo const& rhs);
  GraphicsPipelineInfo& operator=(GraphicsPipelineInfo&& rhs);

  void setShader(Shader const& shader) override;

  void setVertexInput(VertexInfo const& model);

  void setTopology(vk::PrimitiveTopology const& topo);

  void setResolution(vk::Extent2D const& res);

  void setDepthStencil(vk::PipelineDepthStencilStateCreateInfo const& ds);

  void setRasterizer(vk::PipelineRasterizationStateCreateInfo const& raster);

  void setAttachmentBlending(vk::PipelineColorBlendAttachmentState const& attachment, uint32_t i);

  void setPass(vk::RenderPass const& pass, uint32_t subpass);

  void addDynamic(vk::DynamicState const& state);

  vk::PipelineRasterizationStateCreateInfo const& rasterizer() const;
  vk::PipelineDepthStencilStateCreateInfo const& depthStencil() const;

  template<typename U>
  void setSpecConstant(vk::ShaderStageFlagBits const& stage, uint32_t id, U const& value) {
    setSpecConstant(stage, id, sizeof(value), std::addressof(value));
  }
  
  void setSpecConstant(vk::ShaderStageFlagBits const& stage, uint32_t id, size_t size, void const* ptr) {
    auto iter = m_spec_infos.find(stage);
    if (iter != m_spec_infos.end()) {
      iter->second.setSpecConstant(id, size, ptr);
    }
    else {
      throw std::runtime_error{"spec constant shader stage '" + to_string(stage) + "' not existant"};
    }
  }

 private:
  void setShaderStages(std::vector<vk::PipelineShaderStageCreateInfo> const& stages);

  VertexInfo info_vert;
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
  
  std::vector<vk::DynamicState> info_dynamics;

  std::map<vk::ShaderStageFlagBits, SpecInfo> m_spec_infos;
};

#endif