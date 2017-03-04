#include "application_compute.hpp"

#include "app/launcher.hpp"
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
const uint32_t ApplicationCompute::imageCount = 2;

ApplicationCompute::ApplicationCompute(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window, cmdline::parser const& cmd_parse) 
 :ApplicationSingle{resource_path, device, chain, window, cmd_parse}
 ,m_textureSampler{m_device, vkDestroySampler}
 ,m_descriptorPool{m_device, vkDestroyDescriptorPool}
 ,m_descriptorPool_2{m_device, vkDestroyDescriptorPool}
{  
  m_shaders.emplace("scene", Shader{m_device, {m_resource_path + "shaders/quad_vert.spv", m_resource_path + "shaders/transfer_frag.spv"}});

  createTextureImages();
  createTextureSamplers();

  createRenderResources();

  // startRenderThread();
}

ApplicationCompute::~ApplicationCompute() {
  shutDown();
}

FrameResource ApplicationCompute::createFrameResource() {
  auto res = ApplicationSingle::createFrameResource();
  res.command_buffers.emplace("gbuffer", m_device.createCommandBuffer("graphics", vk::CommandBufferLevel::eSecondary));
  return res;
}

void ApplicationCompute::updateResourceCommandBuffers(FrameResource& res) {
  res.command_buffers.at("gbuffer").reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  // first pass
  res.command_buffers.at("gbuffer").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("gbuffer").bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene"));
  res.command_buffers.at("gbuffer").bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene").layout(), 2, {m_descriptor_sets.at("texture")}, {});
  res.command_buffers.at("gbuffer").setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("gbuffer").setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("gbuffer").draw(4, 1, 0, 0);

  res.command_buffers.at("gbuffer").end();
}

void ApplicationCompute::recordDrawBuffer(FrameResource& res) {

  res.command_buffers.at("draw").reset({});

  res.command_buffers.at("draw").begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  res.command_buffers.at("draw").beginRenderPass(m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.command_buffers.at("draw").executeCommands({res.command_buffers.at("gbuffer")});
  
  res.command_buffers.at("draw").endRenderPass();
  // make sure rendering to image is done before blitting
  // barrier is now performed through renderpass dependency

  vk::ImageBlit blit{};
  blit.srcSubresource = img_to_resource_layer(m_images.at("color").info());
  blit.dstSubresource = img_to_resource_layer(m_swap_chain.imgInfo());
  blit.srcOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};
  blit.dstOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};
  res.command_buffers.at("draw").blitImage(m_images.at("color"), m_images.at("color").layout(), m_swap_chain.images().at(res.image), m_swap_chain.layout(), {blit}, vk::Filter::eNearest);

  res.command_buffers.at("draw").end();
}

void ApplicationCompute::createFramebuffers() {
  m_framebuffer = FrameBuffer{m_device, {&m_images.at("color")}, m_render_pass};
}

void ApplicationCompute::createRenderPasses() {
  sub_pass_t pass_1({0},{});
  m_render_pass = RenderPass{m_device, {m_images.at("color").info()}, {pass_1}};
}

void ApplicationCompute::createPipelines() {
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

void ApplicationCompute::updatePipelines() {
  auto info_pipe = m_pipelines.at("scene").info();
  info_pipe.setShader(m_shaders.at("scene"));
  m_pipelines.at("scene").recreate(info_pipe);
}

void ApplicationCompute::createFramebufferAttachments() {
  auto extent = vk::Extent3D{m_swap_chain.extent().width, m_swap_chain.extent().height, 1}; 
 
  m_images["color"] = Image{m_device, extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc};
  m_images.at("color").transitionToLayout(vk::ImageLayout::eTransferSrcOptimal);
  m_allocators.at("images").allocate(m_images.at("color"));
}

void ApplicationCompute::createTextureImages() {
  pixel_data pix_data = texture_loader::file(m_resource_path + "textures/test.tga");

  m_images["texture"] = Image{m_device, pix_data.extent, pix_data.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst};
  m_allocators.at("images").allocate(m_images.at("texture"));
  m_images.at("texture").transitionToLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
  m_device.uploadImageData(pix_data.ptr(), m_images.at("texture"));
}

void ApplicationCompute::createTextureSamplers() {
  m_textureSampler = m_device->createSampler({{}, vk::Filter::eLinear, vk::Filter::eLinear});
}

void ApplicationCompute::updateDescriptors() { 
  m_images.at("texture").writeToSet(m_descriptor_sets.at("texture"), 0, m_textureSampler.get());
}

void ApplicationCompute::createDescriptorPools() {
  m_descriptorPool = m_shaders.at("scene").createPool(1);

  vk::DescriptorSetAllocateInfo allocInfo{};
  allocInfo.descriptorPool = m_descriptorPool;
  allocInfo.descriptorSetCount = std::uint32_t(m_shaders.at("scene").setLayouts().size());
  allocInfo.pSetLayouts = m_shaders.at("scene").setLayouts().data();

  m_descriptor_sets["texture"] = m_device->allocateDescriptorSets(allocInfo)[2];

  // m_descriptorPool_2 = m_shaders.at("quad").createPool(2);
  // allocInfo.descriptorPool = m_descriptorPool_2;
  // allocInfo.descriptorSetCount = std::uint32_t(m_shaders.at("quad").setLayouts().size());
  // allocInfo.pSetLayouts = m_shaders.at("quad").setLayouts().data();

  // m_descriptor_sets["lighting"] = m_device->allocateDescriptorSets(allocInfo)[1];
}

///////////////////////////// misc functions ////////////////////////////////

// exe entry point
int main(int argc, char* argv[]) {
  Launcher::run<ApplicationCompute>(argc, argv);
}