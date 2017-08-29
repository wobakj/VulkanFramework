#include "wrap/conversions.hpp"
#include "wrap/image_res.hpp"
#include "wrap/descriptor_pool.hpp"

#include "frame_resource.hpp"
#include "geometry_loader.hpp"
#include "texture_loader.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "cmdline.h"
#include <vulkan/vulkan.hpp>

#include <iostream>

struct UniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 model;
    glm::mat4 normal;
    glm::vec4 eye_world_space;
};

struct light_t {
  glm::fvec3 position;
  float intensity;
  glm::fvec3 color;
  float radius;
};
const std::size_t NUM_LIGHTS = 128;
struct BufferLights {
  light_t lights[NUM_LIGHTS];
};
BufferLights buff_l;

struct LightGridBufferObject {
  glm::uvec2 grid_size;
  glm::uvec2 tile_size;
  glm::uvec2 resolution;
  float near;
  float far;
  glm::vec4 frustum_corners[4];
};

template<typename T>
cmdline::parser ApplicationClustered<T>::getParser() {
  return T::getParser();
}

template<typename T>
ApplicationClustered<T>::ApplicationClustered(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse)
    : T{resource_path, device, surf, cmd_parse},
      m_tileSize{32, 32},
      m_nearFrustumCornersClipSpace{
          glm::vec4(-1.0f, +1.0f, 0.0f, 1.0f),  // bottom left
          glm::vec4(+1.0f, +1.0f, 0.0f, 1.0f),  // bottom right
          glm::vec4(+1.0f, -1.0f, 0.0f, 1.0f),  // top right
          glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f)   // top left
      } {
  this->m_shaders.emplace("compute", Shader{this->m_device, {this->m_resource_path + "shaders/light_grid_comp.spv"}});
  this->m_shaders.emplace("simple", Shader{this->m_device, {this->m_resource_path + "shaders/simple_world_space_vert.spv", this->m_resource_path + "shaders/simple_frag.spv"}});
  this->m_shaders.emplace("quad", Shader{this->m_device, {this->m_resource_path + "shaders/quad_vert.spv", this->m_resource_path + "shaders/deferred_clustered_pbr_frag.spv"}});
  this->m_shaders.emplace("tonemapping", Shader{this->m_device, {this->m_resource_path + "shaders/fullscreen_vert.spv", this->m_resource_path + "shaders/tone_mapping_frag.spv"}});

  createVertexBuffer();
  createUniformBuffers();
  createLights();  
  createTextureImages();
  createTextureSamplers();

  this->createRenderResources();
}

template<typename T>
ApplicationClustered<T>::~ApplicationClustered() {
  this->shutDown();
}

template<typename T>
FrameResource ApplicationClustered<T>::createFrameResource() {
  auto res = T::createFrameResource();
  res.command_buffers.emplace("compute", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("gbuffer", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("lighting", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("tonemapping", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  return res;
}

template<typename T>
void ApplicationClustered<T>::logic() { 
  if (this->m_camera.changed()) {
    updateView();
  }
}

template<typename T>
void ApplicationClustered<T>::updateLightGrid() {
  auto resolution = glm::uvec2(this->m_swap_chain.extent().width, this->m_swap_chain.extent().height);
  LightGridBufferObject lightGridBuff{};
  // compute number of required screen tiles regarding the tile size and our
  // current resolution; number of depth slices is constant and needs to be
  // the same as in the compute shader
  m_lightGridSize = glm::uvec3{
      (resolution.x + m_tileSize.x - 1) / m_tileSize.x,
      (resolution.y + m_tileSize.y - 1) / m_tileSize.y, 16};
  lightGridBuff.grid_size = glm::uvec2{m_lightGridSize.x, m_lightGridSize.y};

  // compute near frustum corners in view space
  auto invProj = glm::inverse(this->m_camera.projectionMatrix());
  for (unsigned int i = 0; i < 4; ++i) {
    auto corner = invProj * m_nearFrustumCornersClipSpace[i];
    lightGridBuff.frustum_corners[i] = glm::vec4(glm::vec3(corner) / corner.w, 1.0f);
  }

  // some other required values
  lightGridBuff.near = this->m_camera.near();
  lightGridBuff.far = this->m_camera.far();
  lightGridBuff.tile_size = m_tileSize;
  lightGridBuff.resolution = resolution;

  this->m_transferrer.uploadBufferData(&lightGridBuff,
                                 this->m_buffer_views.at("lightgrid"));
}

template<typename T>
void ApplicationClustered<T>::updateResourceCommandBuffers(FrameResource& res) {
  vk::CommandBufferInheritanceInfo inheritanceInfo{};

  // light grid compute
  res.command_buffers.at("compute")->reset({});
  res.command_buffers.at("compute")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});
  res.command_buffers.at("compute")->bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline_compute);
  res.command_buffers.at("compute")->bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, m_pipeline_compute.layout(), 0,
      {this->m_descriptor_sets.at("lightgrid")}, {});

  glm::vec3 workers(8.0f);
  res.command_buffers.at("compute")->dispatch(
      uint32_t(glm::ceil(float(m_lightGridSize.x) / workers.x)),
      uint32_t(glm::ceil(float(m_lightGridSize.y) / workers.y)),
      uint32_t(glm::ceil(float(m_lightGridSize.z) / workers.z)));

  res.command_buffers.at("compute")->end();

  // all following passes use the renderpass and framebuffer
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;

  // first pass
  inheritanceInfo.subpass = 0;
  res.command_buffers.at("gbuffer")->reset({});
  res.command_buffers.at("gbuffer").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("gbuffer")->bindPipeline(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("scene"));
  res.command_buffers.at("gbuffer")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("scene").layout(), 0, {this->m_descriptor_sets.at("matrix"), this->m_descriptor_sets.at("textures")}, {});
  res.command_buffers.at("gbuffer")->setViewport(0, {this->m_swap_chain.asViewport()});
  res.command_buffers.at("gbuffer")->setScissor(0, {this->m_swap_chain.asRect()});

  res.command_buffers.at("gbuffer").bindGeometry(m_model);
  res.command_buffers.at("gbuffer").drawGeometry(1);

  res.command_buffers.at("gbuffer").end();
  //deferred shading pass 
  inheritanceInfo.subpass = 1;
  res.command_buffers.at("lighting")->reset({});
  res.command_buffers.at("lighting")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  vk::ImageMemoryBarrier barrier_image{};
  barrier_image.image = this->m_images.at("light_vol");
  barrier_image.subresourceRange = this->m_images.at("light_vol").view();
  barrier_image.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
  barrier_image.dstAccessMask = vk::AccessFlagBits::eShaderRead;
  barrier_image.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier_image.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier_image.oldLayout = vk::ImageLayout::eGeneral;
  barrier_image.newLayout = vk::ImageLayout::eGeneral;

  res.commandBuffer("lighting")->pipelineBarrier(
    vk::PipelineStageFlagBits::eComputeShader,
    vk::PipelineStageFlagBits::eFragmentShader,
    vk::DependencyFlags{},
    {},
    {},
    {barrier_image}
  );

  res.command_buffers.at("lighting")->bindPipeline(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("quad"));
  res.command_buffers.at("lighting")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("quad").layout(), 0, {this->m_descriptor_sets.at("matrix"), this->m_descriptor_sets.at("lighting")}, {});
  res.command_buffers.at("lighting")->setViewport(0, {this->m_swap_chain.asViewport()});
  res.command_buffers.at("lighting")->setScissor(0, {this->m_swap_chain.asRect()});

  res.command_buffers.at("lighting")->draw(4, 1, 0, 0);

  res.command_buffers.at("lighting")->end();

  // tonemapping
  inheritanceInfo.subpass = 2;
  res.command_buffers.at("tonemapping")->reset({});
  res.command_buffers.at("tonemapping")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("tonemapping")->bindPipeline(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("tonemapping"));
  res.command_buffers.at("tonemapping")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("tonemapping").layout(), 0, {this->m_descriptor_sets.at("tonemapping")}, {});
  res.command_buffers.at("tonemapping")->setViewport(0, {this->m_swap_chain.asViewport()});
  res.command_buffers.at("tonemapping")->setScissor(0, {this->m_swap_chain.asRect()});

  res.command_buffers.at("tonemapping")->draw(3, 1, 0, 0);

  res.command_buffers.at("tonemapping")->end();
}

template<typename T>
void ApplicationClustered<T>::recordDrawBuffer(FrameResource& res) {
  res.command_buffers.at("draw")->reset({});

  res.command_buffers.at("draw")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("compute")});

  res.command_buffers.at("draw")->beginRenderPass(m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("gbuffer")});
  
  res.command_buffers.at("draw")->nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
  // execute lighting buffer
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("lighting")});

  res.command_buffers.at("draw")->nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
  // execute tonemapping buffer
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("tonemapping")});

  res.command_buffers.at("draw")->endRenderPass();

  this->presentCommands(res, this->m_images.at("tonemapping_result").view(), vk::ImageLayout::eTransferSrcOptimal);

  res.command_buffers.at("draw")->end();
}

template<typename T>
void ApplicationClustered<T>::createFramebuffers() {
  m_framebuffer = FrameBuffer{this->m_device, {&this->m_images.at("color"), &this->m_images.at("pos"), &this->m_images.at("normal"), &this->m_images.at("depth"), &this->m_images.at("color_2"), &this->m_images.at("tonemapping_result")}, m_render_pass};
}

template<typename T>
void ApplicationClustered<T>::createRenderPasses() {
  // create renderpass with 3 subpasses
  RenderPassInfo info_pass{3};
  info_pass.setAttachment(0, this->m_images.at("color").format(), vk::ImageLayout::eColorAttachmentOptimal);
  info_pass.setAttachment(1, this->m_images.at("pos").format(), vk::ImageLayout::eColorAttachmentOptimal);
  info_pass.setAttachment(2, this->m_images.at("normal").format(), vk::ImageLayout::eColorAttachmentOptimal);
  info_pass.setAttachment(3, this->m_images.at("depth").format(), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  info_pass.setAttachment(4, this->m_images.at("color_2").format(), vk::ImageLayout::eColorAttachmentOptimal);
  // result will be used after the pass
  info_pass.setAttachment(5, this->m_images.at("tonemapping_result").format(), vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eTransferSrcOptimal);
  //first pass receives attachment 0,1,2 as color, position and normal attachment and attachment 3 as depth attachments 
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
  // third pass receives lighting result (4) and writes to tonemapping result (5)
  info_pass.subPass(2).setColorAttachment(0, 5);
  info_pass.subPass(2).setInputAttachment(0, 4);

  m_render_pass = RenderPass{this->m_device, info_pass};
}

template<typename T>
void ApplicationClustered<T>::createPipelines() {
  GraphicsPipelineInfo info_pipe;
  GraphicsPipelineInfo info_pipe2;
  GraphicsPipelineInfo info_pipe3;

  info_pipe.setResolution(this->m_swap_chain.extent());
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

  info_pipe.setShader(this->m_shaders.at("simple"));
  info_pipe.setVertexInput(m_model.vertexInfo());
  info_pipe.setPass(m_render_pass, 0);
  info_pipe.addDynamic(vk::DynamicState::eViewport);
  info_pipe.addDynamic(vk::DynamicState::eScissor);

  vk::PipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = vk::CompareOp::eLess;
  info_pipe.setDepthStencil(depthStencil);

  info_pipe2.setResolution(this->m_swap_chain.extent());
  info_pipe2.setTopology(vk::PrimitiveTopology::eTriangleStrip);
  
  vk::PipelineRasterizationStateCreateInfo rasterizer2{};
  rasterizer2.lineWidth = 1.0f;
  rasterizer2.cullMode = vk::CullModeFlagBits::eFront;
  info_pipe2.setRasterizer(rasterizer2);

  info_pipe2.setAttachmentBlending(colorBlendAttachment, 0);

  info_pipe2.setShader(this->m_shaders.at("quad"));
  info_pipe2.setPass(m_render_pass, 1);
  info_pipe2.addDynamic(vk::DynamicState::eViewport);
  info_pipe2.addDynamic(vk::DynamicState::eScissor);

  // pipeline for tonemapping
  info_pipe3.setResolution(this->m_swap_chain.extent());
  info_pipe3.setTopology(vk::PrimitiveTopology::eTriangleStrip);
  
  info_pipe3.setRasterizer(rasterizer2);

  info_pipe3.setAttachmentBlending(colorBlendAttachment, 0);

  info_pipe3.setShader(this->m_shaders.at("tonemapping"));
  info_pipe3.setPass(m_render_pass, 2);
  info_pipe3.addDynamic(vk::DynamicState::eViewport);
  info_pipe3.addDynamic(vk::DynamicState::eScissor);

  this->m_pipelines.emplace("scene", GraphicsPipeline{this->m_device, info_pipe, this->m_pipeline_cache});
  this->m_pipelines.emplace("quad", GraphicsPipeline{this->m_device, info_pipe2, this->m_pipeline_cache});
  this->m_pipelines.emplace("tonemapping", GraphicsPipeline{this->m_device, info_pipe3, this->m_pipeline_cache});

  ComputePipelineInfo info_pipe_comp;
  info_pipe_comp.setShader(this->m_shaders.at("compute"));
  m_pipeline_compute = ComputePipeline{this->m_device, info_pipe_comp, this->m_pipeline_cache};
}

template<typename T>
void ApplicationClustered<T>::updatePipelines() {
  auto info_pipe = this->m_pipelines.at("scene").info();
  info_pipe.setShader(this->m_shaders.at("simple"));
  this->m_pipelines.at("scene").recreate(info_pipe);

  auto info_pipe2 = this->m_pipelines.at("quad").info();
  info_pipe2.setShader(this->m_shaders.at("quad"));
  this->m_pipelines.at("quad").recreate(info_pipe2);

  auto info_pipe3 = this->m_pipelines.at("tonemapping").info();
  info_pipe3.setShader(this->m_shaders.at("tonemapping"));
  this->m_pipelines.at("tonemapping").recreate(info_pipe3);

  auto info_pipe_comp = m_pipeline_compute.info();
  info_pipe_comp.setShader(this->m_shaders.at("compute"));
  m_pipeline_compute.recreate(info_pipe_comp);
}

template<typename T>
void ApplicationClustered<T>::createVertexBuffer() {
  vertex_data tri = geometry_loader::obj(this->m_resource_path + "models/sponza.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);
  m_model = Geometry{this->m_transferrer, tri};
}

template<typename T>
void ApplicationClustered<T>::createLights() {
  // one big light for ambient
  buff_l.lights[0].position = glm::vec3(0.0f, 4.0f, 0.0f);
  buff_l.lights[0].radius = 25.0f;
  buff_l.lights[0].color = glm::vec3(0.996, 0.9531, 0.8945);
  buff_l.lights[0].intensity = 50.0f;

  std::uniform_real_distribution<float> dis(0.0f, 1.0f);
  // std::randothis->m_device rd;
  std::mt19937 gen(1);  // rd() as param for different seed
  std::function<float()> rand_float = []() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
  };
  // create random lights
  std::vector<glm::vec3> colors = {
    glm::vec3(1.0f, 0.0f, 0.0f),
    glm::vec3(1.0f, 1.0f, 0.0f),
    glm::vec3(1.0f, 0.0f, 1.0f),
    glm::vec3(0.0f, 1.0f, 1.0f),
    glm::vec3(0.0f, 0.0f, 1.0f)
  };
  for (std::size_t i = 0; i < NUM_LIGHTS - 1; ++i) {
    buff_l.lights[i + 1].position =
        glm::vec3(dis(gen) * 10.0f - 5.0f, dis(gen) * 3.0f + 0.5f,
                  dis(gen) * 26.0f - 13.0f);
    buff_l.lights[i + 1].radius = dis(gen) * 2.0f + 1.0f;
    buff_l.lights[i + 1].color = colors[i % colors.size()];
    buff_l.lights[i + 1].intensity = dis(gen) * 30.0f + 30.0f;
  }

  this->m_transferrer.uploadBufferData(&buff_l, this->m_buffer_views.at("light"));
}

template<typename T>
void ApplicationClustered<T>::createFramebufferAttachments() {
 auto depthFormat = findSupportedFormat(
  this->m_device.physical(),
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );
  auto extent = extent_3d(this->m_swap_chain.extent()); 
  this->m_images["depth"] = BackedImage{this->m_device, extent, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment};
  this->m_transferrer.transitionToLayout(this->m_images.at("depth"), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("depth"));

  this->m_images["color"] = BackedImage{this->m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  this->m_transferrer.transitionToLayout(this->m_images.at("color"), vk::ImageLayout::eColorAttachmentOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("color"));

  this->m_images["pos"] = BackedImage{this->m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  this->m_transferrer.transitionToLayout(this->m_images.at("pos"), vk::ImageLayout::eColorAttachmentOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("pos"));

  this->m_images["normal"] = BackedImage{this->m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  this->m_transferrer.transitionToLayout(this->m_images.at("normal"), vk::ImageLayout::eColorAttachmentOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("normal"));

  this->m_images["color_2"] = BackedImage{this->m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  this->m_transferrer.transitionToLayout(this->m_images.at("color_2"), vk::ImageLayout::eColorAttachmentOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("color_2"));

  this->m_images["tonemapping_result"] = BackedImage{this->m_device, extent, this->m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc};
  this->m_transferrer.transitionToLayout(this->m_images.at("tonemapping_result"), vk::ImageLayout::eTransferSrcOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("tonemapping_result"));

  // update light grid so its extent is computed
  updateLightGrid();
  auto lightVolExtent =
      vk::Extent3D{m_lightGridSize.x, m_lightGridSize.y, m_lightGridSize.z};

  // light volume
  this->m_images["light_vol"] = BackedImage{this->m_device, lightVolExtent, vk::Format::eR32G32B32A32Uint, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage};
  this->m_allocators.at("images").allocate(this->m_images.at("light_vol"));
  this->m_transferrer.transitionToLayout(this->m_images.at("light_vol"), vk::ImageLayout::eGeneral);
}

template<typename T>
void ApplicationClustered<T>::createTextureImages() {
  // test texture
  pixel_data pix_data = texture_loader::file(this->m_resource_path + "textures/test.tga");
  this->m_images["texture"] = BackedImage{this->m_device, pix_data.extent, pix_data.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst};
  // bind and upload test texture data
  this->m_allocators.at("images").allocate(this->m_images.at("texture"));

  this->m_transferrer.uploadImageData(pix_data.ptr(), pix_data.size(), this->m_images.at("texture"), vk::ImageLayout::eShaderReadOnlyOptimal);
}

template<typename T>
void ApplicationClustered<T>::createTextureSamplers() {
  m_sampler = Sampler{this->m_device, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat};
  m_volumeSampler = Sampler{this->m_device, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat};
}

template<typename T>
void ApplicationClustered<T>::updateDescriptors() {
  this->m_images.at("light_vol").view().writeToSet(this->m_descriptor_sets.at("lightgrid"), 0, vk::DescriptorType::eStorageImage);
  this->m_buffer_views.at("lightgrid").writeToSet(this->m_descriptor_sets.at("lightgrid"), 1, vk::DescriptorType::eUniformBuffer);
  this->m_buffer_views.at("lightgrid").writeToSet(this->m_descriptor_sets.at("lighting"), 5, vk::DescriptorType::eUniformBuffer);

  this->m_buffer_views.at("uniform").writeToSet(this->m_descriptor_sets.at("matrix"), 0, vk::DescriptorType::eUniformBuffer);
  this->m_buffer_views.at("light").writeToSet(this->m_descriptor_sets.at("lighting"), 3, vk::DescriptorType::eStorageBuffer);
  this->m_buffer_views.at("light").writeToSet(this->m_descriptor_sets.at("lightgrid"), 2, vk::DescriptorType::eStorageBuffer);
  this->m_buffer_views.at("uniform").writeToSet(this->m_descriptor_sets.at("lightgrid"), 3, vk::DescriptorType::eUniformBuffer);
  
  this->m_images.at("texture").view().writeToSet(this->m_descriptor_sets.at("textures"), 0, m_sampler.get());
  this->m_images.at("light_vol").view().writeToSet(this->m_descriptor_sets.at("lighting"), 4, vk::ImageLayout::eGeneral, m_volumeSampler.get());
  this->m_images.at("light_vol").view().writeToSet(this->m_descriptor_sets.at("lightgrid"), 0, vk::DescriptorType::eStorageImage);
  
  this->m_images.at("color").view().writeToSet(this->m_descriptor_sets.at("lighting"), 0, vk::DescriptorType::eInputAttachment);
  this->m_images.at("pos").view().writeToSet(this->m_descriptor_sets.at("lighting"), 1, vk::DescriptorType::eInputAttachment);
  this->m_images.at("normal").view().writeToSet(this->m_descriptor_sets.at("lighting"), 2, vk::DescriptorType::eInputAttachment);

  this->m_images.at("color_2").view().writeToSet(this->m_descriptor_sets.at("tonemapping"), 0, vk::DescriptorType::eInputAttachment);
}

template<typename T>
void ApplicationClustered<T>::createDescriptorPools() {
  DescriptorPoolInfo info_pool{};
  info_pool.reserve(this->m_shaders.at("simple"), 2);
  info_pool.reserve(this->m_shaders.at("quad"), 1, 2);
  info_pool.reserve(this->m_shaders.at("tonemapping"), 1);
  info_pool.reserve(this->m_shaders.at("compute"), 1);

  this->m_descriptor_pool = DescriptorPool{this->m_device, info_pool};
  this->m_descriptor_sets["matrix"] = this->m_descriptor_pool.allocate(this->m_shaders.at("simple").setLayout(0));
  this->m_descriptor_sets["textures"] = this->m_descriptor_pool.allocate(this->m_shaders.at("simple").setLayout(1));
  this->m_descriptor_sets["lighting"] = this->m_descriptor_pool.allocate(this->m_shaders.at("quad").setLayout(1));
  this->m_descriptor_sets["tonemapping"] = this->m_descriptor_pool.allocate(this->m_shaders.at("tonemapping").setLayout(0));
  this->m_descriptor_sets["lightgrid"] = this->m_descriptor_pool.allocate(this->m_shaders.at("compute").setLayout(0));
}

template<typename T>
void ApplicationClustered<T>::createUniformBuffers() {
  this->m_buffers["uniforms"] = Buffer{this->m_device, (sizeof(UniformBufferObject) + sizeof(BufferLights) + sizeof(LightGridBufferObject)) * 2, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};
  this->m_buffer_views["light"] = BufferView{sizeof(BufferLights), vk::BufferUsageFlagBits::eStorageBuffer};
  this->m_buffer_views["uniform"] = BufferView{sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer};
  this->m_buffer_views["lightgrid"] = BufferView{
      sizeof(LightGridBufferObject), vk::BufferUsageFlagBits::eUniformBuffer};

  this->m_allocators.at("buffers").allocate(this->m_buffers.at("uniforms"));

  this->m_buffer_views.at("lightgrid").bindTo(this->m_buffers.at("uniforms"));
  this->m_buffer_views.at("light").bindTo(this->m_buffers.at("uniforms"));
  this->m_buffer_views.at("uniform").bindTo(this->m_buffers.at("uniforms"));
}

////////////////////////////////////////////////////////////
template<typename T>
void ApplicationClustered<T>::updateView() {
  UniformBufferObject ubo{};
  ubo.model = glm::rotate(glm::scale(glm::mat4(), glm::vec3(0.01f)),
                          glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  ubo.view = this->m_camera.viewMatrix();
  ubo.normal = glm::inverseTranspose(ubo.model);
  ubo.proj = this->m_camera.projectionMatrix();
  ubo.eye_world_space = glm::vec4(this->m_camera.position(), 1.0f);

  this->m_transferrer.uploadBufferData(&ubo, this->m_buffer_views.at("uniform"));
}

///////////////////////////// misc functions ////////////////////////////////
// handle key input
template<typename T>
void ApplicationClustered<T>::keyCallback(int key, int scancode, int action, int mods) {
}
