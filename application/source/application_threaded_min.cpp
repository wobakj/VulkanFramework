#include "application_threaded_min.hpp"

#include "app/launcher_win.hpp"
#include "texture_loader.hpp"
#include "geometry_loader.hpp"

// c++ warpper
#include <vulkan/vulkan.hpp>

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <iostream>
#include <chrono>

cmdline::parser ApplicationThreadedMin::getParser() {
  return ApplicationThreaded::getParser();
}

ApplicationThreadedMin::ApplicationThreadedMin(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :ApplicationThreaded{resource_path, device, surf, cmd_parse}
{  
  m_shaders.emplace("scene", Shader{m_device, {m_resource_path + "shaders/quad_vert.spv", m_resource_path + "shaders/solid_frag.spv"}});

  createDescriptorPools();

  createRenderResources();

  startRenderThread();
}

ApplicationThreadedMin::~ApplicationThreadedMin() {
  shutDown();
}

FrameResource ApplicationThreadedMin::createFrameResource() {
  auto res = ApplicationThreaded::createFrameResource();
  res.command_buffers.emplace("gbuffer", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  return res;
}

void ApplicationThreadedMin::updateResourceCommandBuffers(FrameResource& res) {
  res.command_buffers.at("gbuffer")->reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  // first pass
  res.command_buffers.at("gbuffer")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("gbuffer")->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene"));
  res.command_buffers.at("gbuffer")->setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("gbuffer")->setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("gbuffer")->draw(3, 1, 0, 0);

  res.command_buffers.at("gbuffer")->end();
}

void ApplicationThreadedMin::recordDrawBuffer(FrameResource& res) {

  res.command_buffers.at("draw")->reset({});

  res.command_buffers.at("draw")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  res.command_buffers.at("draw")->beginRenderPass(m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("gbuffer")});
  
  res.command_buffers.at("draw")->endRenderPass();
  // make sure rendering to image is done before blitting
  // barrier is now performed through renderpass dependency

  res.command_buffers.at("draw").transitionLayout(*res.target_view, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
  res.command_buffers.at("draw").copyImage(m_images.at("color").view(), vk::ImageLayout::eTransferSrcOptimal, *res.target_view, vk::ImageLayout::eTransferDstOptimal);
  res.command_buffers.at("draw").transitionLayout(*res.target_view, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

  res.command_buffers.at("draw")->end();
}

void ApplicationThreadedMin::createFramebuffers() {
  m_framebuffer = FrameBuffer{m_device, {&m_images.at("color")}, m_render_pass};
}

void ApplicationThreadedMin::createRenderPasses() {
    // create renderpass with 1 subpasses
  RenderPassInfo info_pass{};
  info_pass.setAttachment(0, m_images.at("color").format(), vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
  info_pass.subPass(0).setColorAttachment(0, 0);
  m_render_pass = RenderPass{m_device, info_pass};
}

void ApplicationThreadedMin::createPipelines() {
  GraphicsPipelineInfo info_pipe;

  info_pipe.setResolution(m_swap_chain.extent());
  info_pipe.setTopology(vk::PrimitiveTopology::eTriangleStrip);
  
  vk::PipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = vk::CullModeFlagBits::eNone;
  info_pipe.setRasterizer(rasterizer);

  vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  info_pipe.setAttachmentBlending(colorBlendAttachment, 0);

  info_pipe.setShader(m_shaders.at("scene"));
  info_pipe.setPass(m_render_pass, 0);
  info_pipe.addDynamic(vk::DynamicState::eViewport);
  info_pipe.addDynamic(vk::DynamicState::eScissor);

  m_pipelines.emplace("scene", GraphicsPipeline{m_device, info_pipe, m_pipeline_cache});
}

void ApplicationThreadedMin::updatePipelines() {
  auto info_pipe = m_pipelines.at("scene").info();
  info_pipe.setShader(m_shaders.at("scene"));
  m_pipelines.at("scene").recreate(info_pipe);
}

void ApplicationThreadedMin::createFramebufferAttachments() {
  auto extent = extent_3d(m_swap_chain.extent()); 
 
  m_images["color"] = ImageRes{m_device, extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc};
  m_allocators.at("images").allocate(m_images.at("color"));
  m_transferrer.transitionToLayout(m_images.at("color"), vk::ImageLayout::eTransferSrcOptimal);
}
///////////////////////////// misc functions ////////////////////////////////

// exe entry point
int main(int argc, char* argv[]) {
  LauncherWin::run<ApplicationThreadedMin>(argc, argv);
}