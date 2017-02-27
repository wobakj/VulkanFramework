#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include "wrapper.hpp"
#include "pipeline_info.hpp"

#include <vulkan/vulkan.hpp>

class Device;

using WrapperPipeline = Wrapper<vk::Pipeline, PipelineInfo>;
class Pipeline : public WrapperPipeline {
 public:
  Pipeline();
  Pipeline(Device const& rhs, PipelineInfo const& info, vk::PipelineCache const& cache = VK_NULL_HANDLE);
  Pipeline(Pipeline && rhs);
  Pipeline(Pipeline const&) = delete;
  ~Pipeline();
  
  Pipeline& operator=(Pipeline const&) = delete;
  Pipeline& operator=(Pipeline&& rhs);

  void swap(Pipeline& rhs);

  void recreate(PipelineInfo const& info);
  // forward info method to prevent trainwreck
  vk::PipelineLayout const& layout() const;

 private:
  void destroy() override;

  Device const* m_device;
  vk::PipelineCache m_cache;
};

#endif