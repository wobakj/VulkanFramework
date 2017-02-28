#include "application_threaded_min.hpp"

#include "launcher.hpp"
#include "texture_loader.hpp"
#include "model_loader.hpp"

// c++ warpper
#include <vulkan/vulkan.hpp>

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <iostream>
#include <chrono>

// child classes must overwrite
const uint32_t ApplicationThreadedMin::imageCount = 3;

ApplicationThreadedMin::ApplicationThreadedMin(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window, cmdline::parser const& cmd_parse) 
 :ApplicationThreaded{resource_path, device, chain, window, cmd_parse}
{  
  m_shaders.emplace("scene", Shader{m_device, {m_resource_path + "shaders/quad_vert.spv", m_resource_path + "shaders/solid_frag.spv"}});

  createRenderResources();

  startRenderThread();
}

ApplicationThreadedMin::~ApplicationThreadedMin() {
  shutDown();
}

FrameResource ApplicationThreadedMin::createFrameResource() {
  auto res = ApplicationThreaded::createFrameResource();
  res.command_buffers.emplace("gbuffer", m_device.createCommandBuffer("graphics", vk::CommandBufferLevel::eSecondary));
  return res;
}

void ApplicationThreadedMin::updateResourceCommandBuffers(FrameResource& res) {
  res.command_buffers.at("gbuffer").reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  // first pass
  res.command_buffers.at("gbuffer").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("gbuffer").bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene"));
  res.command_buffers.at("gbuffer").setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("gbuffer").setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("gbuffer").draw(3, 1, 0, 0);

  res.command_buffers.at("gbuffer").end();
}

void ApplicationThreadedMin::recordDrawBuffer(FrameResource& res) {

  res.command_buffers.at("draw").reset({});

  res.command_buffers.at("draw").begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  res.command_buffers.at("draw").beginRenderPass(m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.command_buffers.at("draw").executeCommands({res.command_buffers.at("gbuffer")});
  
  res.command_buffers.at("draw").endRenderPass();
  // make sure rendering to image is done before blitting
  // barrier is now performed through renderpass dependency

  vk::ImageBlit blit{};
  blit.srcSubresource = img_to_resource_layer(m_images.at("color_2").info());
  blit.dstSubresource = img_to_resource_layer(m_swap_chain.imgInfo());
  blit.srcOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};
  blit.dstOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};
  res.command_buffers.at("draw").blitImage(m_images.at("color_2"), m_images.at("color_2").layout(), m_swap_chain.images().at(res.image), m_swap_chain.layout(), {blit}, vk::Filter::eNearest);

  res.command_buffers.at("draw").end();
}

void ApplicationThreadedMin::createFramebuffers() {
  m_framebuffer = FrameBuffer{m_device, {&m_images.at("color_2")}, m_render_pass};
}

void ApplicationThreadedMin::createRenderPasses() {
  sub_pass_t pass_1({0},{});
  m_render_pass = RenderPass{m_device, {m_images.at("color_2").info()}, {pass_1}};
}

void ApplicationThreadedMin::createPipelines() {
  PipelineInfo info_pipe;

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

  m_pipelines.emplace("scene", Pipeline{m_device, info_pipe, m_pipeline_cache});
}

void ApplicationThreadedMin::updatePipelines() {
  auto info_pipe = m_pipelines.at("scene").info();
  info_pipe.setShader(m_shaders.at("scene"));
  m_pipelines.at("scene").recreate(info_pipe);
}

void ApplicationThreadedMin::createMemoryPools() {
  m_device->waitIdle();
  // allocate pool for 5 32x4 fb attachments
  m_device.reallocateMemoryPool("framebuffer", m_images.at("color_2").memoryTypeBits(), vk::MemoryPropertyFlagBits::eDeviceLocal, m_images.at("color_2").size() * 5);
  
  m_images.at("color_2").bindTo(m_device.memoryPool("framebuffer"));
}

void ApplicationThreadedMin::createFramebufferAttachments() {
  auto extent = vk::Extent3D{m_swap_chain.extent().width, m_swap_chain.extent().height, 1}; 
 
  m_images["color_2"] = m_device.createImage(extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
  m_images.at("color_2").transitionToLayout(vk::ImageLayout::eTransferSrcOptimal);

  createMemoryPools();
}
///////////////////////////// misc functions ////////////////////////////////

// exe entry point
int main(int argc, char* argv[]) {
  Launcher::run<ApplicationThreadedMin>(argc, argv);
}