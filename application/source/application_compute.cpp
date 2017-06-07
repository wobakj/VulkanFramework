#include "application_compute.hpp"

#include "app/launcher_win.hpp"
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

cmdline::parser ApplicationCompute::getParser() {
  return ApplicationSingle::getParser();
}

ApplicationCompute::ApplicationCompute(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :ApplicationSingle{resource_path, device, surf, cmd_parse}
{  
  m_shaders.emplace("scene", Shader{m_device, {m_resource_path + "shaders/quad_vert.spv", m_resource_path + "shaders/transfer_frag.spv"}});
  m_shaders.emplace("compute", Shader{m_device, {m_resource_path + "shaders/pattern_comp.spv"}});

  createTextureImages();
  createTextureSamplers();
  createUniformBuffers();

  createRenderResources();
}

ApplicationCompute::~ApplicationCompute() {
  shutDown();
}

FrameResource ApplicationCompute::createFrameResource() {
  auto res = ApplicationSingle::createFrameResource();
  res.command_buffers.emplace("gbuffer", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("compute", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  return res;
}

void ApplicationCompute::updateResourceCommandBuffers(FrameResource& res) {
  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  res.command_buffers.at("compute")->reset({});
  res.command_buffers.at("compute")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});
  res.command_buffers.at("compute")->bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline_compute);
  res.command_buffers.at("compute")->bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_compute.layout(), 0, {m_descriptor_sets.at("storage")}, {});

  glm::uvec3 dims{m_images.at("texture").extent().width, m_images.at("texture").extent().height, m_images.at("texture").extent().depth};
  glm::uvec3 workers{16, 16, 1};
  // 512^2 threads in blocks of 16^2
  res.command_buffers.at("compute")->dispatch(dims.x / workers.x, dims.y / workers.y, dims.z / workers.z); 

  res.command_buffers.at("compute")->end();

  res.command_buffers.at("gbuffer")->reset({});
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  // first pass
  res.command_buffers.at("gbuffer")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});
  // barrier to make image data visible to vertex shader
  vk::ImageMemoryBarrier barrier_image{};
  barrier_image.image = m_images.at("texture");
  barrier_image.subresourceRange = img_to_resource_range(m_images.at("texture").info());
  barrier_image.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
  barrier_image.dstAccessMask = vk::AccessFlagBits::eShaderRead;
  barrier_image.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier_image.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier_image.oldLayout = vk::ImageLayout::eGeneral;
  barrier_image.newLayout = vk::ImageLayout::eGeneral;

  res.commandBuffer("gbuffer")->pipelineBarrier(
    vk::PipelineStageFlagBits::eComputeShader,
    vk::PipelineStageFlagBits::eFragmentShader,
    vk::DependencyFlags{},
    {},
    {},
    {barrier_image}
  );

  res.command_buffers.at("gbuffer")->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene"));
  res.command_buffers.at("gbuffer")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene").layout(), 2, {m_descriptor_sets.at("texture")}, {});
  res.command_buffers.at("gbuffer")->setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("gbuffer")->setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("gbuffer")->draw(4, 1, 0, 0);

  res.command_buffers.at("gbuffer")->end();
}

void ApplicationCompute::recordDrawBuffer(FrameResource& res) {
  updateUniformBuffers();

  res.command_buffers.at("draw")->reset({});

  res.command_buffers.at("draw")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("compute")});

  res.command_buffers.at("draw")->beginRenderPass(m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("gbuffer")});
  
  res.command_buffers.at("draw")->endRenderPass();
  // make sure rendering to image is done before blitting
  // barrier is now performed through renderpass dependency

  vk::ImageBlit blit{};
  blit.srcSubresource = img_to_resource_layer(m_images.at("color").info());
  blit.dstSubresource = img_to_resource_layer(m_swap_chain.imgInfo());
  blit.srcOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};
  blit.dstOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};

  m_swap_chain.layoutTransitionCommand(res.command_buffers.at("draw").get(), res.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
  res.command_buffers.at("draw")->blitImage(m_images.at("color").get(), m_images.at("color").layout(), m_swap_chain.image(res.image), m_swap_chain.layout(), {blit}, vk::Filter::eNearest);
  m_swap_chain.layoutTransitionCommand(res.command_buffers.at("draw").get(), res.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

  res.command_buffers.at("draw")->end();
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

  ComputePipelineInfo info_pipe_comp;
  info_pipe_comp.setShader(m_shaders.at("compute"));
  m_pipeline_compute = ComputePipeline{m_device, info_pipe_comp, m_pipeline_cache};
}

void ApplicationCompute::updatePipelines() {
  auto info_pipe = m_pipelines.at("scene").info();
  info_pipe.setShader(m_shaders.at("scene"));
  m_pipelines.at("scene").recreate(info_pipe);

  auto info_pipe_comp = m_pipeline_compute.info();
  info_pipe_comp.setShader(m_shaders.at("compute"));
  m_pipeline_compute.recreate(info_pipe_comp);
}

void ApplicationCompute::createFramebufferAttachments() {
  auto extent = vk::Extent3D{m_swap_chain.extent().width, m_swap_chain.extent().height, 1}; 
 
  m_images["color"] = ImageRes{m_device, extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc};
  m_transferrer.transitionToLayout(m_images.at("color"), vk::ImageLayout::eTransferSrcOptimal);
  m_allocators.at("images").allocate(m_images.at("color"));
}

void ApplicationCompute::createTextureImages() {
  m_images["texture"] = ImageRes{m_device, vk::Extent3D{512, 512, 1}, vk::Format::eR32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled
                                                                                                                   | vk::ImageUsageFlagBits::eTransferDst
                                                                                                                   | vk::ImageUsageFlagBits::eStorage};
  m_allocators.at("images").allocate(m_images.at("texture"));
  m_transferrer.transitionToLayout(m_images.at("texture"), vk::ImageLayout::eGeneral);
}

void ApplicationCompute::createTextureSamplers() {
  m_sampler = Sampler{m_device, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat};
}

void ApplicationCompute::updateDescriptors() { 
  m_images.at("texture").view().writeToSet(m_descriptor_sets.at("texture"), 0, m_sampler.get());
  m_images.at("texture").view().writeToSet(m_descriptor_sets.at("storage"), 0, vk::DescriptorType::eStorageImage);
  
  m_buffers.at("time").writeToSet(m_descriptor_sets.at("storage"), 1, vk::DescriptorType::eUniformBuffer);
}

void ApplicationCompute::createDescriptorPools() {
  DescriptorPoolInfo info_pool{};
  info_pool.reserve(m_shaders.at("scene"), 2, 1);
  info_pool.reserve(m_shaders.at("compute"), 0, 1);

  m_descriptor_pool = DescriptorPool{m_device, info_pool};
  m_descriptor_sets["texture"] = m_descriptor_pool.allocate(m_shaders.at("scene").setLayout(2));
  m_descriptor_sets["storage"] = m_descriptor_pool.allocate(m_shaders.at("compute").setLayout(0));
}

void ApplicationCompute::createUniformBuffers() {
  m_buffers["time"] = Buffer{m_device, sizeof(float), vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};
  m_allocators.at("buffers").allocate(m_buffers.at("time"));
}

void ApplicationCompute::updateUniformBuffers() {
  float time = float(glfwGetTime()) * 2.0f;
  m_transferrer.uploadBufferData(&time, m_buffers.at("time"));
}
///////////////////////////// misc functions ////////////////////////////////

// exe entry point
int main(int argc, char* argv[]) {
  LauncherWin::run<ApplicationCompute>(argc, argv);
}