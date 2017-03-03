#ifndef PIPELINE_INFO_HPP
#define PIPELINE_INFO_HPP

#include <vulkan/vulkan.hpp>
#include <vector>

class Shader;

template<typename T>
class PipelineInfo {
 public:
  PipelineInfo()
   :info{}
  {
    info.flags = vk::PipelineCreateFlagBits::eAllowDerivatives;
    info.basePipelineIndex = -1;
  }

  virtual void setShader(Shader const& shader) = 0;

  void setRoot(vk::Pipeline const& root) {
    info.flags |= vk::PipelineCreateFlagBits::eDerivative;
    // insert previously created pipeline here to derive this one from
    info.basePipelineHandle = root;
  }
  
  operator T const&() const {
    return info;
  }

  vk::PipelineLayout const& layout() const {
    return info.layout;
  }

 protected:
  T info;
};

#endif