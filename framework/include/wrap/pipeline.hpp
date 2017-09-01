#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include "wrap/wrapper.hpp"

#include <vulkan/vulkan.hpp>

class Device;

class Pipeline {
 public:
  virtual ~Pipeline() {};

  virtual operator vk::Pipeline const&() const = 0;
    // forward info method to prevent trainwreck
  virtual vk::PipelineLayout const& layout() const = 0;
  virtual vk::PipelineBindPoint bindPoint() const = 0;
};

template<typename T>
class PipelineT : public Wrapper<vk::Pipeline, T>, public Pipeline {
using WrapperPipeline = Wrapper<vk::Pipeline, T>;
 public:
  PipelineT();
  PipelineT(Device const& rhs, T const& info, vk::PipelineCache const& cache = VK_NULL_HANDLE);
  PipelineT(PipelineT && rhs);
  PipelineT(PipelineT const&) = delete;
  ~PipelineT();
  
  PipelineT& operator=(PipelineT const&) = delete;
  PipelineT& operator=(PipelineT&& rhs);

  void swap(PipelineT& rhs);

  operator vk::Pipeline const&() const;

  virtual void recreate(T const& info);
  // forward info method to prevent trainwreck
  vk::PipelineLayout const& layout() const;
  vk::PipelineBindPoint bindPoint() const;
  
 private:
  void destroy() override;

  vk::Device m_device;
  vk::PipelineCache m_cache;
};

#include "wrap/device.hpp"

template<typename T>
PipelineT<T>::PipelineT()
 :WrapperPipeline{}
 ,m_device{nullptr}
 ,m_cache{}
{}

template<typename T>
PipelineT<T>::PipelineT(PipelineT<T> && rhs)
 :WrapperPipeline{}
{
  swap(rhs);
}

template<typename T>
PipelineT<T>::~PipelineT() {
  WrapperPipeline::cleanup();
}

template<typename T>
void PipelineT<T>::destroy() {
  m_device.destroyPipeline(WrapperPipeline::get());
}

template<typename T>
PipelineT<T> & PipelineT<T>::operator=(PipelineT<T>&& pipeline) {
  swap(pipeline);
  return *this;
}

template<typename T>
void PipelineT<T>::swap(PipelineT<T>& rhs) {
  WrapperPipeline::swap(rhs);
  std::swap(m_device, rhs.m_device);
  std::swap(m_cache, rhs.m_cache);
}

template<typename T>
vk::PipelineLayout const& PipelineT<T>::layout() const {
  return WrapperPipeline::info().layout();
}

// specialisations
#include "wrap/pipeline_g_info.hpp"
using GraphicsPipeline = PipelineT<GraphicsPipelineInfo>;

template<>
inline void GraphicsPipeline::recreate(GraphicsPipelineInfo const& info) {
  m_info = info;
  m_info.setRoot(get());
  m_object = m_device.createGraphicsPipelines(m_cache, {m_info})[0];
}

template<>
inline vk::PipelineBindPoint GraphicsPipeline::bindPoint() const {
  return vk::PipelineBindPoint::eGraphics;
}

template<>
inline GraphicsPipeline::PipelineT(Device const& device, GraphicsPipelineInfo const& info, vk::PipelineCache const& cache)
 :WrapperPipeline{std::move(device->createGraphicsPipelines(cache, {info})[0]), info}
 ,m_device{device}
 ,m_cache{cache}
{}

template<>
inline GraphicsPipeline::operator vk::Pipeline const&() const {
  return get();
}

#include "wrap/pipeline_c_info.hpp"
using ComputePipeline = PipelineT<ComputePipelineInfo>;
template<>
inline void ComputePipeline::recreate(ComputePipelineInfo const& info) {
  m_info = info;
  m_info.setRoot(get());
  m_object = m_device.createComputePipelines(m_cache, {m_info})[0];
}

template<>
inline vk::PipelineBindPoint ComputePipeline::bindPoint() const {
  return vk::PipelineBindPoint::eCompute;
}

template<>
inline ComputePipeline::PipelineT(Device const& device, ComputePipelineInfo const& info, vk::PipelineCache const& cache)
 :WrapperPipeline{std::move(device->createComputePipelines(cache, {info})[0]), info}
 ,m_device{device}
 ,m_cache{cache}
{}

template<>
inline ComputePipeline::operator vk::Pipeline const&() const {
  return get();
}

#endif