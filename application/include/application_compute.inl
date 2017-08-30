#include "wrap/descriptor_pool_info.hpp"
#include "texture_loader.hpp"
#include "geometry_loader.hpp"

// c++ warpper
#include <vulkan/vulkan.hpp>

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
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
  this->m_shaders.emplace("compute", Shader{this->m_device, {this->m_resource_path + "shaders/pattern_comp.spv"}});

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
  res.command_buffers.at("compute")->reset({});
  res.command_buffers.at("compute")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});
  res.command_buffers.at("compute")->bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline_compute);
  res.command_buffers.at("compute")->bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_compute.layout(), 0, {this->m_descriptor_sets.at("storage")}, {});

  glm::uvec3 dims{this->m_images.at("texture").extent().width, this->m_images.at("texture").extent().height, this->m_images.at("texture").extent().depth};
  glm::uvec3 workers{16, 16, 1};
  // 512^2 threads in blocks of 16^2
  res.command_buffers.at("compute")->dispatch(dims.x / workers.x, dims.y / workers.y, dims.z / workers.z); 

  res.command_buffers.at("compute")->end();
}

template<typename T>
void ApplicationCompute<T>::recordDrawBuffer(FrameResource& res) {
  updateUniformBuffers();

  res.command_buffers.at("draw")->reset({});

  res.command_buffers.at("draw")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("compute")});

  this->presentCommands(res, this->m_images.at("texture"), vk::ImageLayout::eGeneral);
  
  res.command_buffers.at("draw")->end();
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
  this->m_images.at("texture").view().writeToSet(this->m_descriptor_sets.at("storage"), 0, vk::DescriptorType::eStorageImage);
  
  this->m_buffers.at("time").writeToSet(this->m_descriptor_sets.at("storage"), 1, vk::DescriptorType::eUniformBuffer);
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
