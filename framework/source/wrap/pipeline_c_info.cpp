#include "wrap/pipeline_c_info.hpp"

#include "wrap/shader.hpp"

ComputePipelineInfo::ComputePipelineInfo()
 :PipelineInfo{}
{} 

ComputePipelineInfo::ComputePipelineInfo(ComputePipelineInfo const& rhs)
 :ComputePipelineInfo{}
{
  info.layout = rhs.info.layout;
  info.stage = rhs.info.stage;
  m_spec_info = rhs.m_spec_info;
  info.stage.pSpecializationInfo = &m_spec_info.get();
}

void ComputePipelineInfo::setShader(Shader const& shader) {
  info.layout = shader.get();
  // TODO: better interface for single-stage shader
  info.stage = shader.shaderStages()[0];
  info.stage.pSpecializationInfo = &m_spec_info.get();
}