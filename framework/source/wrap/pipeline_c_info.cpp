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
}

void ComputePipelineInfo::setShader(Shader const& shader) {
  info.layout = shader.get();
  // TODO: better interface for single-stage shader
  info.stage = shader.shaderStages()[0];
}