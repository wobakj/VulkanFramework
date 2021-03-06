#include "wrap/descriptor_pool_info.hpp"
#include "wrap/conversions.hpp"

//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <chrono>

template<typename T>
cmdline::parser ApplicationCompute<T>::getParser() {
  return T::getParser();
}

template<typename T>
ApplicationCompute<T>::ApplicationCompute(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :T{resource_path, device, surf, cmd_parse}
{  
  this->m_shaders.emplace("compute", Shader{this->m_device, {this->resourcePath() + "shaders/pattern_comp.spv"}});

  createTextureImages();
  createUniformBuffers();

  this->createRenderResources();
}

template<typename T>
ApplicationCompute<T>::~ApplicationCompute<T>() {
  this->shutDown();
}

template<typename T>
FrameResource ApplicationCompute<T>::createFrameResource() {
  auto res = T::createFrameResource();
  res.command_buffers.emplace("compute", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  return res;
}

template<typename T>
void ApplicationCompute<T>::updateResourceCommandBuffers(FrameResource& res) {
  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  res.commandBuffer("compute")->reset({});
  res.commandBuffer("compute")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});
  res.commandBuffer("compute")->bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline_compute);
  res.commandBuffer("compute")->bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_compute.layout(), 0, {this->m_descriptor_sets.at("storage")}, {});

  glm::uvec3 dims = vec3(this->m_images.at("texture").extent());
  glm::uvec3 workers{16, 16, 1};
  // 512^2 threads in blocks of 16^2
  res.commandBuffer("compute")->dispatch(dims.x / workers.x, dims.y / workers.y, dims.z / workers.z); 

  res.commandBuffer("compute")->end();
}

template<typename T>
void ApplicationCompute<T>::recordDrawBuffer(FrameResource& res) {
  updateUniformBuffers();

  res.commandBuffer("primary")->reset({});

  res.commandBuffer("primary")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  res.commandBuffer("primary")->executeCommands({res.commandBuffer("compute")});

  res.commandBuffer("primary").transitionLayout(this->m_images.at("texture"), vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
  this->presentCommands(res, this->m_images.at("texture"), vk::ImageLayout::eTransferSrcOptimal);
  res.commandBuffer("primary").transitionLayout(this->m_images.at("texture"), vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eGeneral);
  
  res.commandBuffer("primary")->end();
}

template<typename T>
void ApplicationCompute<T>::createPipelines() {
  ComputePipelineInfo info_pipe_comp;
  info_pipe_comp.setShader(this->m_shaders.at("compute"));
  m_pipeline_compute = ComputePipeline{this->m_device, info_pipe_comp, this->m_pipeline_cache};
}

template<typename T>
void ApplicationCompute<T>::updatePipelines() {
  auto info_pipe_comp = m_pipeline_compute.info();
  info_pipe_comp.setShader(this->m_shaders.at("compute"));
  m_pipeline_compute.recreate(info_pipe_comp);
}

template<typename T>
void ApplicationCompute<T>::createTextureImages() {
    auto extent = vk::Extent3D{1280, 720, 1}; 
  this->m_images["texture"] = BackedImage{this->m_device, extent, vk::Format::eB8G8R8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferSrc
                                                                                                   | vk::ImageUsageFlagBits::eStorage};
  this->m_allocators.at("images").allocate(this->m_images.at("texture"));
  this->m_transferrer.transitionToLayout(this->m_images.at("texture"), vk::ImageLayout::eGeneral);
}

template<typename T>
void ApplicationCompute<T>::updateDescriptors() { 
  this->m_descriptor_sets.at("storage").bind(0, this->m_images.at("texture").view(), vk::DescriptorType::eStorageImage);
  this->m_descriptor_sets.at("storage").bind(1, this->m_buffers.at("time"), vk::DescriptorType::eUniformBuffer);
}

template<typename T>
void ApplicationCompute<T>::createDescriptorPools() {
  DescriptorPoolInfo info_pool{};
  info_pool.reserve(this->m_shaders.at("compute"), 0, 1);

  this->m_descriptor_pool = DescriptorPool{this->m_device, info_pool};
  this->m_descriptor_sets["storage"] = this->m_descriptor_pool.allocate(this->m_shaders.at("compute").setLayout(0));
}

template<typename T>
void ApplicationCompute<T>::createUniformBuffers() {
  this->m_buffers["time"] = Buffer{this->m_device, sizeof(float), vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};
  this->m_allocators.at("buffers").allocate(this->m_buffers.at("time"));
}

template<typename T>
void ApplicationCompute<T>::updateUniformBuffers() {
  float time = float(glfwGetTime()) * 2.0f;
  this->m_transferrer.uploadBufferData(&time, this->m_buffers.at("time"));
}
