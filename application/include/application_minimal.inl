#include "texture_loader.hpp"
#include "geometry_loader.hpp"
#include "wrap/conversions.hpp"

// c++ warpper
#include <vulkan/vulkan.hpp>

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <iostream>
#include <chrono>

template<typename T>
cmdline::parser ApplicationMinimal<T>::getParser() {
  return T::getParser();
}

template<typename T>
ApplicationMinimal<T>::ApplicationMinimal(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :T{resource_path, device, surf, cmd_parse}
{  
  this->m_shaders.emplace("scene", Shader{this->m_device, {this->resourcePath() + "shaders/quad_vert.spv", this->resourcePath() + "shaders/solid_frag.spv"}});

  this->createDescriptorPools();

  this->createRenderResources();
}

template<typename T>
ApplicationMinimal<T>::~ApplicationMinimal<T>() {
  this->shutDown();
}

template<typename T>
FrameResource ApplicationMinimal<T>::createFrameResource() {
  auto res = T::createFrameResource();
  res.command_buffers.emplace("draw", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  return res;
}

template<typename T>
void ApplicationMinimal<T>::updateResourceCommandBuffers(FrameResource& res) {
  res.commandBuffer("draw")->reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = this->m_render_pass;
  inheritanceInfo.framebuffer = this->m_framebuffer;
  inheritanceInfo.subpass = 0;

  // first pass
  res.commandBuffer("draw")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.commandBuffer("draw")->bindPipeline(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("scene"));
  res.commandBuffer("draw")->setViewport(0, viewport(this->resolution()));
  res.commandBuffer("draw")->setScissor(0, rect(this->resolution()));

  res.commandBuffer("draw")->draw(3, 1, 0, 0);

  res.commandBuffer("draw")->end();
}

template<typename T>
void ApplicationMinimal<T>::recordDrawBuffer(FrameResource& res) {

  res.commandBuffer("primary")->reset({});

  res.commandBuffer("primary")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  res.commandBuffer("primary")->beginRenderPass(this->m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute draw buffer
  res.commandBuffer("primary")->executeCommands({res.commandBuffer("draw")});
  
  res.commandBuffer("primary")->endRenderPass();

  this->presentCommands(res, this->m_images.at("color"), vk::ImageLayout::eTransferSrcOptimal);

  res.commandBuffer("primary")->end();
}

template<typename T>
void ApplicationMinimal<T>::createFramebuffers() {
  this->m_framebuffer = FrameBuffer{this->m_device, {&this->m_images.at("color")}, this->m_render_pass};
}

template<typename T>
void ApplicationMinimal<T>::createRenderPasses() {
    // create renderpass with 1 subpasses
  RenderPassInfo info_pass{};
  info_pass.setAttachment(0, this->m_images.at("color").format(), vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
  info_pass.subPass(0).setColorAttachment(0, 0);
  this->m_render_pass = RenderPass{this->m_device, info_pass};
}

template<typename T>
void ApplicationMinimal<T>::createPipelines() {
  GraphicsPipelineInfo info_pipe;

  info_pipe.setResolution(extent_2d(this->resolution()));
  info_pipe.setTopology(vk::PrimitiveTopology::eTriangleStrip);
  
  vk::PipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = vk::CullModeFlagBits::eNone;
  info_pipe.setRasterizer(rasterizer);

  vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  info_pipe.setAttachmentBlending(colorBlendAttachment, 0);

  info_pipe.setShader(this->m_shaders.at("scene"));
  info_pipe.setPass(this->m_render_pass, 0);
  info_pipe.addDynamic(vk::DynamicState::eViewport);
  info_pipe.addDynamic(vk::DynamicState::eScissor);

  this->m_pipelines.emplace("scene", GraphicsPipeline{this->m_device, info_pipe, this->m_pipeline_cache});
}

template<typename T>
void ApplicationMinimal<T>::updatePipelines() {
  auto info_pipe = this->m_pipelines.at("scene").info();
  info_pipe.setShader(this->m_shaders.at("scene"));
  this->m_pipelines.at("scene").recreate(info_pipe);
}

template<typename T>
void ApplicationMinimal<T>::createFramebufferAttachments() {
  auto extent = extent_3d(this->resolution()); 
 
  this->m_images["color"] = BackedImage{this->m_device, extent, vk::Format::eB8G8R8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc};
  this->m_allocators.at("images").allocate(this->m_images.at("color"));
  this->m_transferrer.transitionToLayout(this->m_images.at("color"), vk::ImageLayout::eTransferSrcOptimal);
}
