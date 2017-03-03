#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include "wrap/wrapper.hpp"

#include <vulkan/vulkan.hpp>

class Device;

template<typename T>
class Pipeline : public Wrapper<vk::Pipeline, T>{
using WrapperPipeline = Wrapper<vk::Pipeline, T>;
 public:
  Pipeline();
  Pipeline(Device const& rhs, T const& info, vk::PipelineCache const& cache = VK_NULL_HANDLE);
  Pipeline(Pipeline && rhs);
  Pipeline(Pipeline const&) = delete;
  ~Pipeline();
  
  Pipeline& operator=(Pipeline const&) = delete;
  Pipeline& operator=(Pipeline&& rhs);

  void swap(Pipeline& rhs);

  virtual void recreate(T const& info);
  // forward info method to prevent trainwreck
  vk::PipelineLayout const& layout() const;
  vk::PipelineBindPoint bindPoint() const;
  
 private:
  void destroy() override;

  Device const* m_device;
  vk::PipelineCache m_cache;
};

#include "wrap/device.hpp"

template<typename T>
Pipeline<T>::Pipeline()
 :WrapperPipeline{}
 ,m_device{nullptr}
 ,m_cache{VK_NULL_HANDLE}
{}

template<typename T>
Pipeline<T>::Pipeline(Pipeline<T> && rhs)
 :WrapperPipeline{}
{
  swap(rhs);
}

template<typename T>
Pipeline<T>::~Pipeline() {
  WrapperPipeline::cleanup();
}

template<typename T>
void Pipeline<T>::destroy() {
  (*m_device)->destroyPipeline(WrapperPipeline::get());
}

template<typename T>
Pipeline<T> & Pipeline<T>::operator=(Pipeline<T>&& pipeline) {
  swap(pipeline);
  return *this;
}

template<typename T>
void Pipeline<T>::swap(Pipeline<T>& rhs) {
  WrapperPipeline::swap(rhs);
  std::swap(m_device, rhs.m_device);
  std::swap(m_cache, rhs.m_cache);
}

template<typename T>
vk::PipelineLayout const& Pipeline<T>::layout() const {
  return WrapperPipeline::info().layout();
}

// specialisations
#include "wrap/pipeline_g_info.hpp"
using GraphicsPipeline = Pipeline<GraphicsPipelineInfo>;

template<>
inline void GraphicsPipeline::recreate(GraphicsPipelineInfo const& info) {
  m_info = info;
  m_info.setRoot(get());
  m_object = (*m_device)->createGraphicsPipelines(m_cache, {m_info})[0];
}

template<>
inline vk::PipelineBindPoint GraphicsPipeline::bindPoint() const {
  return vk::PipelineBindPoint::eGraphics;
}

template<>
inline GraphicsPipeline::Pipeline(Device const& device, GraphicsPipelineInfo const& info, vk::PipelineCache const& cache)
 :Pipeline{}
{
  m_device = &device;
  m_cache = cache;
  m_info = info;
  m_object = (*m_device)->createGraphicsPipelines(m_cache, {m_info})[0];
}

#include "wrap/pipeline_c_info.hpp"
using ComputePipeline = Pipeline<ComputePipelineInfo>;
template<>
inline void ComputePipeline::recreate(ComputePipelineInfo const& info) {
  m_info = info;
  m_info.setRoot(get());
  m_object = (*m_device)->createComputePipelines(m_cache, {m_info})[0];
}

template<>
inline vk::PipelineBindPoint ComputePipeline::bindPoint() const {
  return vk::PipelineBindPoint::eCompute;
}

template<>
inline ComputePipeline::Pipeline(Device const& device, ComputePipelineInfo const& info, vk::PipelineCache const& cache)
 :Pipeline{}
{
  m_device = &device;
  m_cache = cache;
  m_info = info;
  m_object = (*m_device)->createComputePipelines(m_cache, {m_info})[0];
}

#endif