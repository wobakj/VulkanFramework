#include "wrap/pipeline_cache.hpp"

#include "wrap/device.hpp"

PipelineCache::PipelineCache()
 :WrapperPipelineCache{}
 ,m_device{nullptr}
{}

PipelineCache::PipelineCache(PipelineCache && PipelineCache)
 :WrapperPipelineCache{}
{
  swap(PipelineCache);
}

PipelineCache::PipelineCache(Device const& device, std::vector<uint8_t> const& data)
 :PipelineCache{}
{
  m_device = &device;
  if (!data.empty()) {
    m_info.initialDataSize = uint32_t(data.size());
    m_info.pInitialData = data.data();
  }
  m_object = (*m_device)->createPipelineCache(m_info);
}

PipelineCache::~PipelineCache() {
  cleanup();
}

void PipelineCache::destroy() {
  (*m_device)->destroyPipelineCache(get());
}

PipelineCache& PipelineCache::operator=(PipelineCache&& PipelineCache) {
  swap(PipelineCache);
  return *this;
}

void PipelineCache::swap(PipelineCache& PipelineCache) {
  WrapperPipelineCache::swap(PipelineCache);
  std::swap(m_device, PipelineCache.m_device);
}