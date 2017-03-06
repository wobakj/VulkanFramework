#include "application_clustered.hpp"

#include "app/launcher.hpp"
#include "wrap/descriptor_pool_info.hpp"
#include "texture_loader.hpp"
#include "model_loader.hpp"

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <iostream>

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 normal;
};

struct light_t {
  glm::fvec3 position;
  float pad = 0.0f;
  glm::fvec3 color;
  float radius;
};
const std::size_t NUM_LIGHTS = 32;
struct BufferLights {
  glm::uvec3 lightGridSize;
  light_t lights[NUM_LIGHTS];
};
BufferLights buff_l;


cmdline::parser ApplicationClustered::getParser() {
  cmdline::parser cmd_parse{};
  return cmd_parse;
}
// child classes must overwrite
const uint32_t ApplicationClustered::imageCount = 2;

ApplicationClustered::ApplicationClustered(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window, cmdline::parser const& cmd_parse) 
 :ApplicationSingle{resource_path, device, chain, window, cmd_parse}
 ,m_textureSampler{m_device, vkDestroySampler}
 ,m_light_grid{m_camera.near(), m_camera.far(), m_camera.projectionMatrix(), glm::uvec2(chain.extent().width, chain.extent().height)}
 ,m_data_light_volume(m_light_grid.dimensions().x * m_light_grid.dimensions().y * m_light_grid.dimensions().z, -1)
{

  m_shaders.emplace("simple", Shader{m_device, {m_resource_path + "shaders/simple_vert.spv", m_resource_path + "shaders/simple_frag.spv"}});
  m_shaders.emplace("quad", Shader{m_device, {m_resource_path + "shaders/quad_vert.spv", m_resource_path + "shaders/deferred_clustered_frag.spv"}});

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
  res.command_buffers.emplace("gbuffer", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("lighting", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  return res;
}

void ApplicationClustered::logic() { 
  if (m_camera.changed()) {
    updateView();
    updateLightVolume();
  }
}

void ApplicationClustered::updateLightVolume() {
  m_data_light_volume.resize(m_light_grid.dimensions().x *
                             m_light_grid.dimensions().y *
                             m_light_grid.dimensions().z);
  // update light colume data
  for (uint32_t z = 0; z < m_light_grid.dimensions().z; ++z)
    for (uint32_t x = 0; x < m_light_grid.dimensions().x; ++x)
      for (uint32_t y = 0; y < m_light_grid.dimensions().y; ++y)
        for (unsigned int i = 0; i < NUM_LIGHTS; ++i) {
          auto lightPos = glm::vec4(buff_l.lights[i].position, 1.0f);
          auto lightPosViewSpaceVec4 = m_camera.viewMatrix() * lightPos;
          auto lightPosViewSpace =
              glm::vec3(lightPosViewSpaceVec4) / lightPosViewSpaceVec4.w;
          if (m_light_grid.pointFroxelDistance(x, y, z, lightPosViewSpace) <
              buff_l.lights[i].radius)
            m_data_light_volume.at(
                z * m_light_grid.dimensions().y * m_light_grid.dimensions().x +
                y * m_light_grid.dimensions().x + x) |= 1 << i;
        }

  m_transferrer.uploadImageData(m_data_light_volume.data(),
                           m_images.at("light_vol"));

  buff_l.lightGridSize = m_light_grid.dimensions();
  m_transferrer.uploadBufferData(&buff_l, m_buffer_views.at("light"));
}

void ApplicationClustered::updateResourceCommandBuffers(FrameResource& res) {
  res.command_buffers.at("gbuffer")->reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  // first pass
  res.command_buffers.at("gbuffer")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("gbuffer")->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene"));
  res.command_buffers.at("gbuffer")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene").layout(), 0, {m_descriptor_sets.at("matrix"), m_descriptor_sets.at("textures")}, {});
  res.command_buffers.at("gbuffer")->setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("gbuffer")->setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("gbuffer")->bindVertexBuffers(0, {m_model.buffer()}, {0});
  res.command_buffers.at("gbuffer")->bindIndexBuffer(m_model.buffer(), m_model.indexOffset(), vk::IndexType::eUint32);

  res.command_buffers.at("gbuffer")->drawIndexed(m_model.numIndices(), 1, 0, 0, 0);

  res.command_buffers.at("gbuffer")->end();
  //deferred shading pass 
  inheritanceInfo.subpass = 1;
  res.command_buffers.at("lighting")->reset({});
  res.command_buffers.at("lighting")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("lighting")->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines.at("quad"));
  res.command_buffers.at("lighting")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines.at("quad").layout(), 0, {m_descriptor_sets.at("matrix"), m_descriptor_sets.at("lighting")}, {});
  res.command_buffers.at("lighting")->setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("lighting")->setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("lighting")->draw(4, 1, 0, 0);

  res.command_buffers.at("lighting")->end();
}

void ApplicationClustered::recordDrawBuffer(FrameResource& res) {
  res.command_buffers.at("draw")->reset({});

  res.command_buffers.at("draw")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  res.command_buffers.at("draw")->beginRenderPass(m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("gbuffer")});
  
  res.command_buffers.at("draw")->nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
  // execute lighting buffer
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("lighting")});

  res.command_buffers.at("draw")->endRenderPass();
  // make sure rendering to image is done before blitting
  // barrier is now performed through renderpass dependency

  vk::ImageBlit blit{};
  blit.srcSubresource = img_to_resource_layer(m_images.at("color_2").info());
  blit.dstSubresource = img_to_resource_layer(m_swap_chain.imgInfo());
  blit.srcOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};
  blit.dstOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};

  m_swap_chain.layoutTransitionCommand(res.command_buffers.at("draw").get(), res.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
  res.command_buffers.at("draw")->blitImage(m_images.at("color_2"), m_images.at("color_2").layout(), m_swap_chain.image(res.image), m_swap_chain.layout(), {blit}, vk::Filter::eNearest);
  m_swap_chain.layoutTransitionCommand(res.command_buffers.at("draw").get(), res.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

  res.command_buffers.at("draw")->end();
}

void ApplicationClustered::createFramebuffers() {
  m_framebuffer = FrameBuffer{m_device, {&m_images.at("color"), &m_images.at("pos"), &m_images.at("normal"), &m_images.at("depth"), &m_images.at("color_2")}, m_render_pass};
}

void ApplicationClustered::createRenderPasses() {
  //first pass receives attachment 0,1,2 as color, position and normal attachment and attachment 3 as depth attachments 
  sub_pass_t pass_1({0, 1, 2},{},3);
  // second pass receives attachments 0,1,2 and inputs and writes to 4
  sub_pass_t pass_2({4},{0,1,2}, 3);
  m_render_pass = RenderPass{m_device, {m_images.at("color").info(), m_images.at("pos").info(), m_images.at("normal").info(), m_images.at("depth").info(), m_images.at("color_2").info()}, {pass_1, pass_2}};
}

void ApplicationClustered::createPipelines() {
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

  info_pipe.setShader(m_shaders.at("simple"));
  info_pipe.setVertexInput(m_model);
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

  m_pipelines.emplace("scene", GraphicsPipeline{m_device, info_pipe, m_pipeline_cache});
  m_pipelines.emplace("quad", GraphicsPipeline{m_device, info_pipe2, m_pipeline_cache});
}

void ApplicationClustered::updatePipelines() {
  auto info_pipe = m_pipelines.at("scene").info();
  info_pipe.setShader(m_shaders.at("simple"));
  m_pipelines.at("scene").recreate(info_pipe);

  auto info_pipe2 = m_pipelines.at("quad").info();
  info_pipe2.setShader(m_shaders.at("quad"));
  m_pipelines.at("quad").recreate(info_pipe2);
}

void ApplicationClustered::createVertexBuffer() {
  vertex_data tri = model_loader::obj(m_resource_path + "models/house.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);
  m_model = Geometry{m_transferrer, tri};
}

void ApplicationClustered::createLights() {
  std::srand(5);
  for (std::size_t i = 0; i < NUM_LIGHTS; ++i) {
    light_t light;
    light.position = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)} * 25.0f - 12.5f;
    light.color = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)};
    light.radius = float(rand()) / float(RAND_MAX) * 5.0f + 5.0f;
    buff_l.lights[i] = light;
  }
  // m_transferrer.uploadBufferData(&buff_l, m_buffer_views.at("light"));
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

  m_images["color"] = Image{m_device, extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("color"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("color"));

  m_images["pos"] = Image{m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("pos"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("pos"));

  m_images["normal"] = Image{m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("normal"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("normal"));

  m_images["color_2"] = Image{m_device, extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc};
  m_transferrer.transitionToLayout(m_images.at("color_2"), vk::ImageLayout::eTransferSrcOptimal);
  m_allocators.at("images").allocate(m_images.at("color_2"));

  // light volume
  m_images["light_vol"] = Image{m_device, m_light_grid.extent(), vk::Format::eR32Uint, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst};
  m_allocators.at("images").allocate(m_images.at("light_vol"));
  m_transferrer.transitionToLayout(m_images.at("light_vol"), vk::ImageLayout::eShaderReadOnlyOptimal);
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
  m_textureSampler = m_device->createSampler({{}, vk::Filter::eLinear, vk::Filter::eLinear});
  m_volumeSampler = m_device->createSampler({{}, vk::Filter::eNearest, vk::Filter::eNearest});
}

void ApplicationClustered::updateDescriptors() {
  m_buffer_views.at("uniform").writeToSet(m_descriptor_sets.at("matrix"), 0);
  m_buffer_views.at("light").writeToSet(m_descriptor_sets.at("lighting"), 3);
  
  m_images.at("texture").writeToSet(m_descriptor_sets.at("textures"), 0, m_textureSampler.get());
  m_images.at("light_vol").writeToSet(m_descriptor_sets.at("lighting"), 4, m_volumeSampler.get());
  
  m_images.at("color").writeToSet(m_descriptor_sets.at("lighting"), 0, vk::DescriptorType::eInputAttachment);
  m_images.at("pos").writeToSet(m_descriptor_sets.at("lighting"), 1, vk::DescriptorType::eInputAttachment);
  m_images.at("normal").writeToSet(m_descriptor_sets.at("lighting"), 2, vk::DescriptorType::eInputAttachment);
}

void ApplicationClustered::createDescriptorPools() {
  DescriptorPoolInfo info_pool{};
  info_pool.reserve(m_shaders.at("simple"), 2);
  info_pool.reserve(m_shaders.at("quad"), 1, 2);

  m_descriptor_pool = DescriptorPool{m_device, info_pool};
  m_descriptor_sets["matrix"] = m_descriptor_pool.allocate(m_shaders.at("simple"), 0);
  m_descriptor_sets["textures"] = m_descriptor_pool.allocate(m_shaders.at("simple"), 1);
  m_descriptor_sets["lighting"] = m_descriptor_pool.allocate(m_shaders.at("quad"), 1);
}

void ApplicationClustered::createUniformBuffers() {
  m_buffers["uniforms"] = Buffer{m_device, (sizeof(UniformBufferObject) + sizeof(BufferLights)) * 2, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst};
  m_buffer_views["light"] = BufferView{sizeof(BufferLights)};
  m_buffer_views["uniform"] = BufferView{sizeof(UniformBufferObject)};

  m_allocators.at("buffers").allocate(m_buffers.at("uniforms"));

  m_buffer_views.at("light").bindTo(m_buffers.at("uniforms"));
  m_buffer_views.at("uniform").bindTo(m_buffers.at("uniforms"));
}

///////////////////////////// update functions ////////////////////////////////
void ApplicationClustered::updateView() {
  UniformBufferObject ubo{};
  ubo.model = glm::mat4();
  ubo.view = m_camera.viewMatrix();
  ubo.normal = glm::inverseTranspose(ubo.view * ubo.model);
  ubo.proj = m_camera.projectionMatrix();

  m_transferrer.uploadBufferData(&ubo, m_buffer_views.at("uniform"));
}

void ApplicationClustered::onResize(std::size_t width, std::size_t height) {
  // recompute froxels
  m_light_grid.update(m_camera.projectionMatrix(), glm::uvec2(width, height));
}

///////////////////////////// misc functions ////////////////////////////////
// handle key input
void ApplicationClustered::keyCallback(int key, int scancode, int action, int mods) {
}

// exe entry point
int main(int argc, char* argv[]) {
  Launcher::run<ApplicationClustered>(argc, argv);
}