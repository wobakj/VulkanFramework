#ifndef PIPELINEC_CACHE_HPP
#define PIPELINEC_CACHE_HPP

#include "wrapper.hpp"
// #include "pipelineCache_info.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>

class Device;

using WrapperPipelineCache = Wrapper<vk::PipelineCache, vk::PipelineCacheCreateInfo>;
class PipelineCache : public WrapperPipelineCache {
 public:
  PipelineCache();
  PipelineCache(Device const& rhs, std::vector<uint8_t> const& data = std::vector<uint8_t>{});
  PipelineCache(PipelineCache && rhs);
  PipelineCache(PipelineCache const&) = delete;
  ~PipelineCache();
  
  PipelineCache& operator=(PipelineCache const&) = delete;
  PipelineCache& operator=(PipelineCache&& rhs);

  void swap(PipelineCache& rhs);

 private:
  void destroy() override;

  Device const* m_device;
};

#endif