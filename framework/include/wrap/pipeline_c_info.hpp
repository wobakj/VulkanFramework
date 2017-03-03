#ifndef PIPELINE_C_INFO_HPP
#define PIPELINE_C_INFO_HPP

#include "wrap/pipeline_info.hpp"

#include <vulkan/vulkan.hpp>
#include <vector>

class Geometry;
class GeometryLod;

class ComputePipelineInfo : public PipelineInfo<vk::ComputePipelineCreateInfo> {
 public:
  ComputePipelineInfo();
  ComputePipelineInfo(ComputePipelineInfo const&);

  void setShader(Shader const& shader) override;
};

#endif