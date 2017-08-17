#include "application_threaded_simple.hpp"

#include "app/launcher_win.hpp"
#include "wrap/descriptor_pool_info.hpp"
#include "texture_loader.hpp"
#include "geometry_loader.hpp"
#include "wrap/conversions.hpp"

#include <vulkan/vulkan.hpp>

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <chrono>

#define THREADING

struct UniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 model;
    glm::mat4 normal;
} ubo_cam;

struct light_t {
  glm::fvec3 position;
  float intensity;
  glm::fvec3 color;
  float radius;
};
const std::size_t NUM_LIGHTS = 60;
struct BufferLights {
  light_t lights[NUM_LIGHTS];
};
BufferLights buff_l;

cmdline::parser ApplicationThreadedSimple::getParser() {
  return ApplicationThreaded::getParser();
}

ApplicationThreadedSimple::ApplicationThreadedSimple(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :ApplicationThreaded{resource_path, device, surf, cmd_parse}
 ,m_sphere{true}
 ,m_model_dirty{false}
{  
  m_shaders.emplace("scene", Shader{m_device, {m_resource_path + "shaders/simple_vert.spv", m_resource_path + "shaders/simple_frag.spv"}});
  m_shaders.emplace("lights", Shader{m_device, {m_resource_path + "shaders/lighting_vert.spv", m_resource_path + "shaders/deferred_blinn_frag.spv"}});

  createVertexBuffer();
  createUniformBuffers();
  createLights();  
  createTextureImage();
  createTextureSampler();

  createRenderResources();
}

ApplicationThreadedSimple::~ApplicationThreadedSimple() {
  shutDown();
}

FrameResource ApplicationThreadedSimple::createFrameResource() {
  auto res = ApplicationThreaded::createFrameResource();
  res.command_buffers.emplace("gbuffer", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("lighting", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.buffer_views["uniform"] = BufferView{sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer};
  res.buffer_views.at("uniform").bindTo(m_buffers.at("uniforms"));
  return res;
}

void ApplicationThreadedSimple::updateModel() {
  emptyDrawQueue();
  m_sphere = false;
  for (auto& res : m_frame_resources) {
    updateResourceCommandBuffers(res);
  }
  m_model_dirty = false;
  #ifdef THREADING
  if(m_thread_load.joinable()) {
    m_thread_load.join();
  }
  else {
    throw std::runtime_error{"could not join thread"};
  }
  #endif
}

void ApplicationThreadedSimple::logic() {
  if(m_model_dirty.is_lock_free()) {
    if(m_model_dirty) {
      updateModel();
    }
  }
}

void ApplicationThreadedSimple::updateResourceDescriptors(FrameResource& res) {
  res.buffer_views.at("uniform").writeToSet(res.descriptor_sets.at("matrix"), 0, vk::DescriptorType::eUniformBuffer);
}

void ApplicationThreadedSimple::updateResourceCommandBuffers(FrameResource& res) {

  res.command_buffers.at("gbuffer")->reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  // first pass
  res.command_buffers.at("gbuffer").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("gbuffer")->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene"));
  res.command_buffers.at("gbuffer")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene").layout(), 0, {res.descriptor_sets.at("matrix"), m_descriptor_sets.at("textures")}, {});
  res.command_buffers.at("gbuffer")->setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("gbuffer")->setScissor(0, {m_swap_chain.asRect()});
  // choose between sphere and house
  Geometry const* model = nullptr;
  if (m_sphere) {
    model = &m_model;
  }
  else {
    model = &m_model_2;
  }

  res.command_buffers.at("gbuffer").bindGeometry(*model);
  res.command_buffers.at("gbuffer").drawGeometry(1);

  res.command_buffers.at("gbuffer").end();
  //deferred shading pass 
  inheritanceInfo.subpass = 1;
  res.command_buffers.at("lighting")->reset({});
  res.command_buffers.at("lighting").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("lighting")->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines.at("lights"));
  res.command_buffers.at("lighting")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines.at("lights").layout(), 0, {res.descriptor_sets.at("matrix"), m_descriptor_sets.at("lighting")}, {});
  res.command_buffers.at("lighting")->setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("lighting")->setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("lighting").bindGeometry(m_model);
  res.command_buffers.at("lighting").drawGeometry(NUM_LIGHTS);

  res.command_buffers.at("lighting").end();
}

void ApplicationThreadedSimple::recordDrawBuffer(FrameResource& res) {

  res.command_buffers.at("draw")->reset({});

  res.command_buffers.at("draw")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  // always update, because last update could have been to other frame
  updateView();
  res.command_buffers.at("draw")->updateBuffer(res.buffer_views.at("uniform").buffer(), res.buffer_views.at("uniform").offset(), sizeof(ubo_cam), &ubo_cam);
  // barrier to make new data visible to vertex shader
  res.command_buffers.at("draw").bufferBarrier(res.buffer_views.at("uniform").buffer(), 
    vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite, 
    vk::PipelineStageFlagBits::eVertexShader, vk::AccessFlagBits::eUniformRead
  );

  res.command_buffers.at("draw")->beginRenderPass(m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("gbuffer")});
  
  res.command_buffers.at("draw")->nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
  // execute lighting buffer
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("lighting")});

  res.command_buffers.at("draw")->endRenderPass();
  // make sure rendering to image is done before blitting
  // barrier is now performed through renderpass dependency

  res.command_buffers.at("draw").transitionLayout(*res.target_view, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
  res.command_buffers.at("draw").copyImage(m_images.at("color_2").view(), vk::ImageLayout::eTransferSrcOptimal, *res.target_view, vk::ImageLayout::eTransferDstOptimal);
  res.command_buffers.at("draw").transitionLayout(*res.target_view, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

  res.command_buffers.at("draw")->end();
}

void ApplicationThreadedSimple::createFramebuffers() {
  m_framebuffer = FrameBuffer{m_device, {&m_images.at("color"), &m_images.at("pos"), &m_images.at("normal"), &m_images.at("depth"), &m_images.at("color_2")}, m_render_pass};
}

void ApplicationThreadedSimple::createRenderPasses() {
  // create renderpass with 2 subpasses
  RenderPassInfo info_pass{2};
  info_pass.setAttachment(0, m_images.at("color").format(), vk::ImageLayout::eColorAttachmentOptimal);
  info_pass.setAttachment(1, m_images.at("pos").format(), vk::ImageLayout::eColorAttachmentOptimal);
  info_pass.setAttachment(2, m_images.at("normal").format(), vk::ImageLayout::eColorAttachmentOptimal);
  info_pass.setAttachment(3, m_images.at("depth").format(), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  info_pass.setAttachment(4, m_images.at("color_2").format(), vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
  info_pass.subPass(0).setColorAttachment(0, 0);
  info_pass.subPass(0).setColorAttachment(1, 1);
  info_pass.subPass(0).setColorAttachment(2, 2);
  info_pass.subPass(0).setDepthAttachment(3);
  // second pass receives attachments 0,1,2 and inputs and writes to 4
  info_pass.subPass(1).setColorAttachment(0, 4);
  info_pass.subPass(1).setInputAttachment(0, 0);
  info_pass.subPass(1).setInputAttachment(1, 1);
  info_pass.subPass(1).setInputAttachment(2, 2);
  info_pass.subPass(1).setDepthAttachment(3);
  m_render_pass = RenderPass{m_device, info_pass};
}

void ApplicationThreadedSimple::createPipelines() {
  GraphicsPipelineInfo info_pipe;
  GraphicsPipelineInfo info_pipe2;

  info_pipe.setResolution(m_swap_chain.extent());
  info_pipe.setTopology(vk::PrimitiveTopology::eTriangleList);
  
  vk::PipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = vk::CullModeFlagBits::eBack;
  info_pipe.setRasterizer(rasterizer);

  vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  info_pipe.setAttachmentBlending(colorBlendAttachment, 0);
  info_pipe.setAttachmentBlending(colorBlendAttachment, 1);
  info_pipe.setAttachmentBlending(colorBlendAttachment, 2);

  info_pipe.setShader(m_shaders.at("scene"));
  info_pipe.setVertexInput(m_model.vertexInfo());
  info_pipe.setPass(m_render_pass, 0);
  info_pipe.addDynamic(vk::DynamicState::eViewport);
  info_pipe.addDynamic(vk::DynamicState::eScissor);

  vk::PipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = vk::CompareOp::eLess;
  info_pipe.setDepthStencil(depthStencil);

  info_pipe2.setResolution(m_swap_chain.extent());
  info_pipe2.setTopology(vk::PrimitiveTopology::eTriangleList);
  
  vk::PipelineRasterizationStateCreateInfo rasterizer2{};
  rasterizer2.lineWidth = 1.0f;
  rasterizer2.cullMode = vk::CullModeFlagBits::eFront;
  info_pipe2.setRasterizer(rasterizer2);

  vk::PipelineColorBlendAttachmentState colorBlendAttachment2{};
  colorBlendAttachment2.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment2.blendEnable = VK_TRUE;
  colorBlendAttachment2.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
  colorBlendAttachment2.dstColorBlendFactor = vk::BlendFactor::eDstAlpha;
  colorBlendAttachment2.colorBlendOp = vk::BlendOp::eAdd;
  colorBlendAttachment2.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment2.dstAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment2.alphaBlendOp = vk::BlendOp::eAdd;
  info_pipe2.setAttachmentBlending(colorBlendAttachment2, 0);

  info_pipe2.setVertexInput(m_model.vertexInfo());
  info_pipe2.setShader(m_shaders.at("lights"));
  info_pipe2.setPass(m_render_pass, 1);
  info_pipe2.addDynamic(vk::DynamicState::eViewport);
  info_pipe2.addDynamic(vk::DynamicState::eScissor);

  vk::PipelineDepthStencilStateCreateInfo depthStencil2{};
  depthStencil2.depthTestEnable = VK_TRUE;
  depthStencil2.depthWriteEnable = VK_FALSE;
  depthStencil2.depthCompareOp = vk::CompareOp::eGreater;
  info_pipe2.setDepthStencil(depthStencil2);

  m_pipelines.emplace("scene", GraphicsPipeline{m_device, info_pipe, m_pipeline_cache});
  m_pipelines.emplace("lights", GraphicsPipeline{m_device, info_pipe2, m_pipeline_cache});
}

void ApplicationThreadedSimple::updatePipelines() {
  auto info_pipe = m_pipelines.at("scene").info();
  info_pipe.setShader(m_shaders.at("scene"));
  m_pipelines.at("scene").recreate(info_pipe);

  auto info_pipe2 = m_pipelines.at("lights").info();
  info_pipe2.setShader(m_shaders.at("lights"));
  m_pipelines.at("lights").recreate(info_pipe2);
}

void ApplicationThreadedSimple::createVertexBuffer() {
  vertex_data tri = geometry_loader::obj(m_resource_path + "models/sphere.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);

  m_model = Geometry{m_transferrer, tri};
}
void ApplicationThreadedSimple::loadModel() {
  try {
    vertex_data tri = geometry_loader::obj(m_resource_path + "models/house.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);
    m_model_2 = Geometry{m_transferrer, tri};
    m_model_dirty = true;
  }
  catch (std::exception& e) {
    assert(0);
  }
}

void ApplicationThreadedSimple::createLights() {
  std::srand(5);
  for (std::size_t i = 0; i < NUM_LIGHTS; ++i) {
    light_t light;
    light.position = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)} * 25.0f - 12.5f;
    light.color = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)};
    light.radius = float(rand()) / float(RAND_MAX) * 5.0f + 5.0f;
    light.intensity = float(rand()) / float(RAND_MAX);
    buff_l.lights[i] = light;
  }
  m_transferrer.uploadBufferData(&buff_l, m_buffer_views.at("light"));
}

void ApplicationThreadedSimple::createFramebufferAttachments() {
 auto depthFormat = findSupportedFormat(
  m_device.physical(),
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );
  auto extent = extent_3d(m_swap_chain.extent()); 
  m_images["depth"] = ImageRes{m_device, extent, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment};
  m_transferrer.transitionToLayout(m_images.at("depth"), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("depth"));

  m_images["color"] = ImageRes{m_device, extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("color"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("color"));

  m_images["pos"] = ImageRes{m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("pos"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("pos"));

  m_images["normal"] = ImageRes{m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("normal"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("normal"));

  m_images["color_2"] = ImageRes{m_device, extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc};
  m_transferrer.transitionToLayout(m_images.at("color_2"), vk::ImageLayout::eTransferSrcOptimal);
  m_allocators.at("images").allocate(m_images.at("color_2"));
}

void ApplicationThreadedSimple::createTextureImage() {
  pixel_data pix_data = texture_loader::file(m_resource_path + "textures/test.tga");

  m_images["texture"] = ImageRes{m_device, pix_data.extent, pix_data.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst};
  m_allocators.at("images").allocate(m_images.at("texture"));
  
  m_transferrer.uploadImageData(pix_data.ptr(), m_images.at("texture"), vk::ImageLayout::eShaderReadOnlyOptimal);
}

void ApplicationThreadedSimple::createTextureSampler() {
  m_sampler = Sampler{m_device, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat};
}

void ApplicationThreadedSimple::updateDescriptors() {
  m_images.at("texture").view().writeToSet(m_descriptor_sets.at("textures"), 0, m_sampler.get());

  m_images.at("color").view().writeToSet(m_descriptor_sets.at("lighting"), 0, vk::DescriptorType::eInputAttachment);
  m_images.at("pos").view().writeToSet(m_descriptor_sets.at("lighting"), 1, vk::DescriptorType::eInputAttachment);
  m_images.at("normal").view().writeToSet(m_descriptor_sets.at("lighting"), 2, vk::DescriptorType::eInputAttachment);
  m_buffer_views.at("light").writeToSet(m_descriptor_sets.at("lighting"), 3, vk::DescriptorType::eStorageBuffer);
}

void ApplicationThreadedSimple::createDescriptorPools() {
  DescriptorPoolInfo info_pool{};
  info_pool.reserve(m_shaders.at("scene"), 0, m_swap_chain.numImages() - 1);
  info_pool.reserve(m_shaders.at("scene"), 1, 1);
  info_pool.reserve(m_shaders.at("lights"), 1, 1);

  m_descriptor_pool = DescriptorPool{m_device, info_pool};
  m_descriptor_sets["textures"] = m_descriptor_pool.allocate(m_shaders.at("scene").setLayout(1));
  m_descriptor_sets["lighting"] = m_descriptor_pool.allocate(m_shaders.at("lights").setLayout(1));
  
  for(auto& res : m_frame_resources) {
    res.descriptor_sets["matrix"] = m_descriptor_pool.allocate(m_shaders.at("scene").setLayout(0));
  }
}

void ApplicationThreadedSimple::createUniformBuffers() {
  m_buffers["uniforms"] = Buffer{m_device, sizeof(BufferLights) * 4, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};
  m_allocators.at("buffers").allocate(m_buffers.at("uniforms"));

  m_buffer_views["light"] = BufferView{sizeof(BufferLights), vk::BufferUsageFlagBits::eStorageBuffer};
  m_buffer_views.at("light").bindTo(m_buffers.at("uniforms"));
}

///////////////////////////// update functions ////////////////////////////////
void ApplicationThreadedSimple::updateView() {
  ubo_cam.model = glm::mat4();
  ubo_cam.view = m_camera.viewMatrix();
  ubo_cam.normal = glm::inverseTranspose(ubo_cam.view * ubo_cam.model);
  ubo_cam.proj = m_camera.projectionMatrix();

  // m_transferrer.uploadBufferData(&ubo_cam, m_buffers.at("uniform"));
}

///////////////////////////// misc functions ////////////////////////////////
// handle key input
void ApplicationThreadedSimple::keyCallback(int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_L && action == GLFW_PRESS) {
    if (m_sphere) {
    #ifdef THREADING
      // prevent thread creation form being triggered mutliple times
      if (!m_thread_load.joinable()) {
        m_thread_load = std::thread(&ApplicationThreadedSimple::loadModel, this);
      }
    #else
      loadModel();
    #endif
    }
  }
}

// exe entry point
int main(int argc, char* argv[]) {
  LauncherWin::run<ApplicationThreadedSimple>(argc, argv);
}