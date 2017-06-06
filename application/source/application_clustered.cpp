#include "application_clustered.hpp"

#include "app/launcher.hpp"
#include "wrap/descriptor_pool_info.hpp"
#include "texture_loader.hpp"
#include "geometry_loader.hpp"

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

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

cmdline::parser ApplicationClustered::getParser() {
  return ApplicationSingle::getParser();
}

ApplicationClustered::ApplicationClustered(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse)
    : ApplicationSingle{resource_path, device, surf, cmd_parse},
      m_tileSize{32, 32},
      m_nearFrustumCornersClipSpace{
          glm::vec4(-1.0f, +1.0f, 0.0f, 1.0f),  // bottom left
          glm::vec4(+1.0f, +1.0f, 0.0f, 1.0f),  // bottom right
          glm::vec4(+1.0f, -1.0f, 0.0f, 1.0f),  // top right
          glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f)   // top left
      } {
  m_shaders.emplace("compute", Shader{m_device, {m_resource_path + "shaders/light_grid_comp.spv"}});
  m_shaders.emplace("simple", Shader{m_device, {m_resource_path + "shaders/simple_world_space_vert.spv", m_resource_path + "shaders/simple_frag.spv"}});
  m_shaders.emplace("quad", Shader{m_device, {m_resource_path + "shaders/quad_vert.spv", m_resource_path + "shaders/deferred_clustered_pbr_frag.spv"}});
  m_shaders.emplace("tonemapping", Shader{m_device, {m_resource_path + "shaders/fullscreen_vert.spv", m_resource_path + "shaders/tone_mapping_frag.spv"}});

  createVertexBuffer();
  createUniformBuffers();
  createLights();  
  createTextureImages();
  createTextureSamplers();

  createRenderResources();
}

ApplicationClustered::~ApplicationClustered() {
  shutDown();
}

FrameResource ApplicationClustered::createFrameResource() {
  auto res = ApplicationSingle::createFrameResource();
  res.command_buffers.emplace("compute", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("gbuffer", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("lighting", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("tonemapping", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  return res;
}

void ApplicationClustered::logic() { 
  if (m_camera.changed()) {
    updateView();
  }
}

void ApplicationClustered::updateLightGrid() {
  auto resolution = glm::uvec2(m_swap_chain.extent().width, m_swap_chain.extent().height);
  LightGridBufferObject lightGridBuff{};
  // compute number of required screen tiles regarding the tile size and our
  // current resolution; number of depth slices is constant and needs to be
  // the same as in the compute shader
  m_lightGridSize = glm::uvec3{
      (resolution.x + m_tileSize.x - 1) / m_tileSize.x,
      (resolution.y + m_tileSize.y - 1) / m_tileSize.y, 16};
  lightGridBuff.grid_size = glm::uvec2{m_lightGridSize.x, m_lightGridSize.y};

  // compute near frustum corners in view space
  auto invProj = glm::inverse(m_camera.projectionMatrix());
  for (unsigned int i = 0; i < 4; ++i) {
    auto corner = invProj * m_nearFrustumCornersClipSpace[i];
    lightGridBuff.frustum_corners[i] = glm::vec4(glm::vec3(corner) / corner.w, 1.0f);
  }

  // some other required values
  lightGridBuff.near = m_camera.near();
  lightGridBuff.far = m_camera.far();
  lightGridBuff.tile_size = m_tileSize;
  lightGridBuff.resolution = resolution;

  m_transferrer.uploadBufferData(&lightGridBuff,
                                 m_buffer_views.at("lightgrid"));
}

void ApplicationClustered::updateResourceCommandBuffers(FrameResource& res) {
  vk::CommandBufferInheritanceInfo inheritanceInfo{};

  // light grid compute
  res.command_buffers.at("compute")->reset({});
  res.command_buffers.at("compute")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});
  res.command_buffers.at("compute")->bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline_compute);
  res.command_buffers.at("compute")->bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, m_pipeline_compute.layout(), 0,
      {m_descriptor_sets.at("lightgrid")}, {});

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

  res.command_buffers.at("gbuffer")->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene"));
  res.command_buffers.at("gbuffer")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene").layout(), 0, {m_descriptor_sets.at("matrix"), m_descriptor_sets.at("textures")}, {});
  res.command_buffers.at("gbuffer")->setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("gbuffer")->setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("gbuffer").bindGeometry(m_model);
  res.command_buffers.at("gbuffer").drawGeometry(1);

  res.command_buffers.at("gbuffer").end();
  //deferred shading pass 
  inheritanceInfo.subpass = 1;
  res.command_buffers.at("lighting")->reset({});
  res.command_buffers.at("lighting")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  vk::ImageMemoryBarrier barrier_image{};
  barrier_image.image = m_images.at("light_vol");
  barrier_image.subresourceRange = img_to_resource_range(m_images.at("light_vol").info());
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

  res.command_buffers.at("lighting")->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines.at("quad"));
  res.command_buffers.at("lighting")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines.at("quad").layout(), 0, {m_descriptor_sets.at("matrix"), m_descriptor_sets.at("lighting")}, {});
  res.command_buffers.at("lighting")->setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("lighting")->setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("lighting")->draw(4, 1, 0, 0);

  res.command_buffers.at("lighting")->end();

  // tonemapping
  inheritanceInfo.subpass = 2;
  res.command_buffers.at("tonemapping")->reset({});
  res.command_buffers.at("tonemapping")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("tonemapping")->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines.at("tonemapping"));
  res.command_buffers.at("tonemapping")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines.at("tonemapping").layout(), 0, {m_descriptor_sets.at("tonemapping")}, {});
  res.command_buffers.at("tonemapping")->setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("tonemapping")->setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("tonemapping")->draw(3, 1, 0, 0);

  res.command_buffers.at("tonemapping")->end();
}

void ApplicationClustered::recordDrawBuffer(FrameResource& res) {
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
  // make sure rendering to image is done before blitting
  // barrier is now performed through renderpass dependency

  vk::ImageBlit blit{};
  blit.srcSubresource = img_to_resource_layer(m_images.at("tonemapping_result").info());
  blit.dstSubresource = img_to_resource_layer(m_swap_chain.imgInfo());
  blit.srcOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};
  blit.dstOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};

  m_swap_chain.layoutTransitionCommand(res.command_buffers.at("draw").get(), res.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
  res.command_buffers.at("draw")->blitImage(m_images.at("tonemapping_result"), m_images.at("tonemapping_result").layout(), m_swap_chain.image(res.image), m_swap_chain.layout(), {blit}, vk::Filter::eNearest);
  m_swap_chain.layoutTransitionCommand(res.command_buffers.at("draw").get(), res.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

  res.command_buffers.at("draw")->end();
}

void ApplicationClustered::createFramebuffers() {
  m_framebuffer = FrameBuffer{m_device, {&m_images.at("color"), &m_images.at("pos"), &m_images.at("normal"), &m_images.at("depth"), &m_images.at("color_2"), &m_images.at("tonemapping_result")}, m_render_pass};
}

void ApplicationClustered::createRenderPasses() {
  //first pass receives attachment 0,1,2 as color, position and normal attachment and attachment 3 as depth attachments 
  sub_pass_t pass_1({0, 1, 2},{},3);
  // second pass receives attachments 0,1,2 and inputs and writes to 4
  sub_pass_t pass_2({4},{0,1,2}, 3);
  // third pass receives lighting result (4) and writes to tonemapping result (5)
  sub_pass_t pass_3({5},{4});
  m_render_pass = RenderPass{m_device, {m_images.at("color").info(), m_images.at("pos").info(), m_images.at("normal").info(), m_images.at("depth").info(), m_images.at("color_2").info(), m_images.at("tonemapping_result").info()}, {pass_1, pass_2, pass_3}};
}

void ApplicationClustered::createPipelines() {
  GraphicsPipelineInfo info_pipe;
  GraphicsPipelineInfo info_pipe2;
  GraphicsPipelineInfo info_pipe3;

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

  info_pipe.setShader(m_shaders.at("simple"));
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
  info_pipe2.setTopology(vk::PrimitiveTopology::eTriangleStrip);
  
  vk::PipelineRasterizationStateCreateInfo rasterizer2{};
  rasterizer2.lineWidth = 1.0f;
  rasterizer2.cullMode = vk::CullModeFlagBits::eFront;
  info_pipe2.setRasterizer(rasterizer2);

  info_pipe2.setAttachmentBlending(colorBlendAttachment, 0);

  info_pipe2.setShader(m_shaders.at("quad"));
  info_pipe2.setPass(m_render_pass, 1);
  info_pipe2.addDynamic(vk::DynamicState::eViewport);
  info_pipe2.addDynamic(vk::DynamicState::eScissor);

  // pipeline for tonemapping
  info_pipe3.setResolution(m_swap_chain.extent());
  info_pipe3.setTopology(vk::PrimitiveTopology::eTriangleStrip);
  
  info_pipe3.setRasterizer(rasterizer2);

  info_pipe3.setAttachmentBlending(colorBlendAttachment, 0);

  info_pipe3.setShader(m_shaders.at("tonemapping"));
  info_pipe3.setPass(m_render_pass, 2);
  info_pipe3.addDynamic(vk::DynamicState::eViewport);
  info_pipe3.addDynamic(vk::DynamicState::eScissor);

  m_pipelines.emplace("scene", GraphicsPipeline{m_device, info_pipe, m_pipeline_cache});
  m_pipelines.emplace("quad", GraphicsPipeline{m_device, info_pipe2, m_pipeline_cache});
  m_pipelines.emplace("tonemapping", GraphicsPipeline{m_device, info_pipe3, m_pipeline_cache});

  ComputePipelineInfo info_pipe_comp;
  info_pipe_comp.setShader(m_shaders.at("compute"));
  m_pipeline_compute = ComputePipeline{m_device, info_pipe_comp, m_pipeline_cache};
}

void ApplicationClustered::updatePipelines() {
  auto info_pipe = m_pipelines.at("scene").info();
  info_pipe.setShader(m_shaders.at("simple"));
  m_pipelines.at("scene").recreate(info_pipe);

  auto info_pipe2 = m_pipelines.at("quad").info();
  info_pipe2.setShader(m_shaders.at("quad"));
  m_pipelines.at("quad").recreate(info_pipe2);

  auto info_pipe3 = m_pipelines.at("tonemapping").info();
  info_pipe3.setShader(m_shaders.at("tonemapping"));
  m_pipelines.at("tonemapping").recreate(info_pipe3);

  auto info_pipe_comp = m_pipeline_compute.info();
  info_pipe_comp.setShader(m_shaders.at("compute"));
  m_pipeline_compute.recreate(info_pipe_comp);
}

void ApplicationClustered::createVertexBuffer() {
  vertex_data tri = geometry_loader::obj(m_resource_path + "models/sponza.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);
  m_model = Geometry{m_transferrer, tri};
}

void ApplicationClustered::createLights() {
  // one big light for ambient
  buff_l.lights[0].position = glm::vec3(0.0f, 4.0f, 0.0f);
  buff_l.lights[0].radius = 25.0f;
  buff_l.lights[0].color = glm::vec3(0.996, 0.9531, 0.8945);
  buff_l.lights[0].intensity = 50.0f;

  std::uniform_real_distribution<float> dis(0.0f, 1.0f);
  // std::random_device rd;
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

  m_transferrer.uploadBufferData(&buff_l, m_buffer_views.at("light"));
}

void ApplicationClustered::createFramebufferAttachments() {
 auto depthFormat = findSupportedFormat(
  m_device.physical(),
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );
  auto extent = vk::Extent3D{m_swap_chain.extent().width, m_swap_chain.extent().height, 1}; 
  m_images["depth"] = Image{m_device, extent, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment};
  m_transferrer.transitionToLayout(m_images.at("depth"), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("depth"));

  m_images["color"] = Image{m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("color"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("color"));

  m_images["pos"] = Image{m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("pos"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("pos"));

  m_images["normal"] = Image{m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("normal"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("normal"));

  m_images["color_2"] = Image{m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("color_2"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("color_2"));

  m_images["tonemapping_result"] = Image{m_device, extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc};
  m_transferrer.transitionToLayout(m_images.at("tonemapping_result"), vk::ImageLayout::eTransferSrcOptimal);
  m_allocators.at("images").allocate(m_images.at("tonemapping_result"));

  // update light grid so its extent is computed
  updateLightGrid();
  auto lightVolExtent =
      vk::Extent3D{m_lightGridSize.x, m_lightGridSize.y, m_lightGridSize.z};

  // light volume
  m_images["light_vol"] = Image{m_device, lightVolExtent, vk::Format::eR32G32B32A32Uint, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage};
  m_allocators.at("images").allocate(m_images.at("light_vol"));
  m_transferrer.transitionToLayout(m_images.at("light_vol"), vk::ImageLayout::eGeneral);
}

void ApplicationClustered::createTextureImages() {
  // test texture
  pixel_data pix_data = texture_loader::file(m_resource_path + "textures/test.tga");
  m_images["texture"] = Image{m_device, pix_data.extent, pix_data.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst};
  // bind and upload test texture data
  m_allocators.at("images").allocate(m_images.at("texture"));

  m_transferrer.transitionToLayout(m_images.at("texture"), vk::ImageLayout::eShaderReadOnlyOptimal);
  m_transferrer.uploadImageData(pix_data.ptr(), m_images.at("texture"));
}

void ApplicationClustered::createTextureSamplers() {
  m_sampler = Sampler{m_device, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat};
  m_volumeSampler = Sampler{m_device, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat};
}

void ApplicationClustered::updateDescriptors() {
  m_images.at("light_vol").writeToSet(m_descriptor_sets.at("lightgrid"), 0, vk::DescriptorType::eStorageImage);
  m_buffer_views.at("lightgrid").writeToSet(m_descriptor_sets.at("lightgrid"), 1, vk::DescriptorType::eUniformBuffer);
  m_buffer_views.at("lightgrid").writeToSet(m_descriptor_sets.at("lighting"), 5, vk::DescriptorType::eUniformBuffer);

  m_buffer_views.at("uniform").writeToSet(m_descriptor_sets.at("matrix"), 0, vk::DescriptorType::eUniformBuffer);
  m_buffer_views.at("light").writeToSet(m_descriptor_sets.at("lighting"), 3, vk::DescriptorType::eStorageBuffer);
  m_buffer_views.at("light").writeToSet(m_descriptor_sets.at("lightgrid"), 2, vk::DescriptorType::eStorageBuffer);
  m_buffer_views.at("uniform").writeToSet(m_descriptor_sets.at("lightgrid"), 3, vk::DescriptorType::eUniformBuffer);
  
  m_images.at("texture").writeToSet(m_descriptor_sets.at("textures"), 0, m_sampler.get());
  m_images.at("light_vol").writeToSet(m_descriptor_sets.at("lighting"), 4, m_volumeSampler.get());
  m_images.at("light_vol").writeToSet(m_descriptor_sets.at("lightgrid"), 0, vk::DescriptorType::eStorageImage);
  
  m_images.at("color").writeToSet(m_descriptor_sets.at("lighting"), 0, vk::DescriptorType::eInputAttachment);
  m_images.at("pos").writeToSet(m_descriptor_sets.at("lighting"), 1, vk::DescriptorType::eInputAttachment);
  m_images.at("normal").writeToSet(m_descriptor_sets.at("lighting"), 2, vk::DescriptorType::eInputAttachment);

  m_images.at("color_2").writeToSet(m_descriptor_sets.at("tonemapping"), 0, vk::DescriptorType::eInputAttachment);
}

void ApplicationClustered::createDescriptorPools() {
  DescriptorPoolInfo info_pool{};
  info_pool.reserve(m_shaders.at("simple"), 2);
  info_pool.reserve(m_shaders.at("quad"), 1, 2);
  info_pool.reserve(m_shaders.at("tonemapping"), 1);
  info_pool.reserve(m_shaders.at("compute"), 1);

  m_descriptor_pool = DescriptorPool{m_device, info_pool};
  m_descriptor_sets["matrix"] = m_descriptor_pool.allocate(m_shaders.at("simple").setLayout(0));
  m_descriptor_sets["textures"] = m_descriptor_pool.allocate(m_shaders.at("simple").setLayout(1));
  m_descriptor_sets["lighting"] = m_descriptor_pool.allocate(m_shaders.at("quad").setLayout(1));
  m_descriptor_sets["tonemapping"] = m_descriptor_pool.allocate(m_shaders.at("tonemapping").setLayout(0));
  m_descriptor_sets["lightgrid"] = m_descriptor_pool.allocate(m_shaders.at("compute").setLayout(0));
}

void ApplicationClustered::createUniformBuffers() {
  m_buffers["uniforms"] = Buffer{m_device, (sizeof(UniformBufferObject) + sizeof(BufferLights) + sizeof(LightGridBufferObject)) * 2, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};
  m_buffer_views["light"] = BufferView{sizeof(BufferLights), vk::BufferUsageFlagBits::eStorageBuffer};
  m_buffer_views["uniform"] = BufferView{sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer};
  m_buffer_views["lightgrid"] = BufferView{
      sizeof(LightGridBufferObject), vk::BufferUsageFlagBits::eUniformBuffer};

  m_allocators.at("buffers").allocate(m_buffers.at("uniforms"));

  m_buffer_views.at("lightgrid").bindTo(m_buffers.at("uniforms"));
  m_buffer_views.at("light").bindTo(m_buffers.at("uniforms"));
  m_buffer_views.at("uniform").bindTo(m_buffers.at("uniforms"));
}

///////////////////////////// update functions ////////////////////////////////
void ApplicationClustered::updateView() {
  UniformBufferObject ubo{};
  ubo.model = glm::rotate(glm::scale(glm::mat4(), glm::vec3(0.01f)),
                          glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  ubo.view = m_camera.viewMatrix();
  ubo.normal = glm::inverseTranspose(ubo.model);
  ubo.proj = m_camera.projectionMatrix();
  ubo.eye_world_space = glm::vec4(m_camera.position(), 1.0f);

  m_transferrer.uploadBufferData(&ubo, m_buffer_views.at("uniform"));
}

///////////////////////////// misc functions ////////////////////////////////
// handle key input
void ApplicationClustered::keyCallback(int key, int scancode, int action, int mods) {
}

// exe entry point
int main(int argc, char* argv[]) {
  Launcher::run<ApplicationClustered>(argc, argv);
}
