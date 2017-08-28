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
  this->m_shaders.emplace("scene", Shader{this->m_device, {this->m_resource_path + "shaders/quad_vert.spv", this->m_resource_path + "shaders/solid_frag.spv"}});

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
  res.command_buffers.emplace("gbuffer", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  return res;
}

template<typename T>
void ApplicationMinimal<T>::updateResourceCommandBuffers(FrameResource& res) {
  res.command_buffers.at("gbuffer")->reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = this->m_render_pass;
  inheritanceInfo.framebuffer = this->m_framebuffer;
  inheritanceInfo.subpass = 0;

  // first pass
  res.command_buffers.at("gbuffer")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("gbuffer")->bindPipeline(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("scene"));
  res.command_buffers.at("gbuffer")->setViewport(0, {this->m_swap_chain.asViewport()});
  res.command_buffers.at("gbuffer")->setScissor(0, {this->m_swap_chain.asRect()});

  res.command_buffers.at("gbuffer")->draw(3, 1, 0, 0);

  res.command_buffers.at("gbuffer")->end();
}

template<typename T>
void ApplicationMinimal<T>::recordDrawBuffer(FrameResource& res) {

  res.command_buffers.at("draw")->reset({});

  res.command_buffers.at("draw")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  res.command_buffers.at("draw")->beginRenderPass(this->m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("gbuffer")});
  
  res.command_buffers.at("draw")->endRenderPass();

  this->presentCommands(res, this->m_images.at("color").view(), vk::ImageLayout::eTransferSrcOptimal);

  res.command_buffers.at("draw")->end();
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

  info_pipe.setResolution(this->m_swap_chain.extent());
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
  auto extent = extent_3d(this->m_swap_chain.extent()); 
 
  this->m_images["color"] = ImageRes{this->m_device, extent, this->m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc};
  this->m_allocators.at("images").allocate(this->m_images.at("color"));
  this->m_transferrer.transitionToLayout(this->m_images.at("color"), vk::ImageLayout::eTransferSrcOptimal);
}
