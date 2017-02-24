#include "pipeline.hpp"

#include "device.hpp"

Pipeline::Pipeline()
 :WrapperPipeline{}
 ,m_device{nullptr}
 ,m_cache{VK_NULL_HANDLE}
{}

Pipeline::Pipeline(Pipeline && Pipeline)
 :WrapperPipeline{}
{
  swap(Pipeline);
}

Pipeline::Pipeline(Device const& device, PipelineInfo const& info, vk::PipelineCache const& cache)
 :Pipeline{}
{
  m_device = &device;
  m_cache = cache;
  m_info = info;
  m_object = (*m_device)->createGraphicsPipelines(m_cache, {m_info})[0];
}

Pipeline::~Pipeline() {
  cleanup();
}

void Pipeline::destroy() {
  (*m_device)->destroyPipeline(get());
}

Pipeline& Pipeline::operator=(Pipeline&& Pipeline) {
  swap(Pipeline);
  return *this;
}

void Pipeline::recreate(PipelineInfo const& info) {
  m_info = info;
  m_info.setRoot(get());
  m_object = (*m_device)->createGraphicsPipelines(m_cache, {m_info})[0];
}

void Pipeline::swap(Pipeline& Pipeline) {
  WrapperPipeline::swap(Pipeline);
  std::swap(m_device, Pipeline.m_device);
  std::swap(m_cache, Pipeline.m_cache);
}