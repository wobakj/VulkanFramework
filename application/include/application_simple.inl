#include "app/launcher_win.hpp"
#include "wrap/descriptor_pool_info.hpp"
#include "texture_loader.hpp"
#include "geometry_loader.hpp"
#include "wrap/conversions.hpp"

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <iostream>

// #define THREADING

struct UniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 model;
    glm::mat4 normal;
};

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

template<typename T>
cmdline::parser ApplicationSimple<T>::getParser() {
  return T::getParser();
}

template<typename T>
ApplicationSimple<T>::ApplicationSimple(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :T{resource_path, device, surf, cmd_parse}
{

  this->m_shaders.emplace("scene", Shader{this->m_device, {this->m_resource_path + "shaders/simple_vert.spv", this->m_resource_path + "shaders/simple_frag.spv"}});
  this->m_shaders.emplace("lights", Shader{this->m_device, {this->m_resource_path + "shaders/lighting_vert.spv", this->m_resource_path + "shaders/deferred_blinn_frag.spv"}});

  createVertexBuffer();
  createUniformBuffers();
  createLights();  
  createTextureImage();
  createTextureSampler();

  this->createRenderResources();
}

template<typename T>
ApplicationSimple<T>::~ApplicationSimple() {
  this->shutDown();
}

template<typename T>
FrameResource ApplicationSimple<T>::createFrameResource() {
  auto res = T::createFrameResource();
  res.command_buffers.emplace("gbuffer", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("lighting", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  return res;
}

template<typename T>
void ApplicationSimple<T>::logic() {
  if (this->m_camera.changed()) {
    updateView();
  }
}

template<typename T>
void ApplicationSimple<T>::updateResourceCommandBuffers(FrameResource& res) {
  res.commandBuffer("gbuffer")->reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  // first pass
  res.commandBuffer("gbuffer").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.commandBuffer("gbuffer").bindPipeline(this->m_pipelines.at("scene"));
  res.commandBuffer("gbuffer").bindDescriptorSets(0, {this->m_descriptor_sets.at("matrix"), this->m_descriptor_sets.at("textures")}, {});
  // glm::fvec3 test{0.0f, 1.0f, 0.0f};
  // res.commandBuffer("gbuffer").pushConstants(vk::ShaderStageFlagBits::eFragment, 0, test);
  res.commandBuffer("gbuffer")->setViewport(0, {this->m_swap_chain.asViewport()});
  res.commandBuffer("gbuffer")->setScissor(0, {this->m_swap_chain.asRect()});
  res.commandBuffer("gbuffer").bindGeometry(m_model_2);

  res.commandBuffer("gbuffer").drawGeometry();

  res.commandBuffer("gbuffer").end();
  //deferred shading pass 
  inheritanceInfo.subpass = 1;
  res.commandBuffer("lighting")->reset({});
  res.commandBuffer("lighting").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.commandBuffer("lighting").bindPipeline(this->m_pipelines.at("lights"));
  res.commandBuffer("lighting")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("lights").layout(), 0, {this->m_descriptor_sets.at("matrix"), this->m_descriptor_sets.at("lighting")}, {});
  res.commandBuffer("lighting")->setViewport(0, {this->m_swap_chain.asViewport()});
  res.commandBuffer("lighting")->setScissor(0, {this->m_swap_chain.asRect()});

  res.commandBuffer("lighting").bindGeometry(this->m_model);

  res.commandBuffer("lighting").drawGeometry(NUM_LIGHTS);

  res.commandBuffer("lighting").end();
}

template<typename T>
void ApplicationSimple<T>::recordDrawBuffer(FrameResource& res) {
  res.commandBuffer("draw")->reset({});

  res.commandBuffer("draw")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  res.commandBuffer("draw")->beginRenderPass(m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.commandBuffer("draw")->executeCommands({res.commandBuffer("gbuffer")});
  
  res.commandBuffer("draw")->nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
  // execute lighting buffer
  res.commandBuffer("draw")->executeCommands({res.commandBuffer("lighting")});

  res.commandBuffer("draw")->endRenderPass();

  res.commandBuffer("draw").transitionLayout(*res.target_view, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
  res.commandBuffer("draw").copyImage(this->m_images.at("color_2").view(), vk::ImageLayout::eTransferSrcOptimal, *res.target_view, vk::ImageLayout::eTransferDstOptimal);
  res.commandBuffer("draw").transitionLayout(*res.target_view, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

  res.commandBuffer("draw")->end();
}

template<typename T>
void ApplicationSimple<T>::createFramebuffers() {
  m_framebuffer = FrameBuffer{this->m_device, {&this->m_images.at("color"), &this->m_images.at("pos"), &this->m_images.at("normal"), &this->m_images.at("depth"), &this->m_images.at("color_2")}, m_render_pass};
}

template<typename T>
void ApplicationSimple<T>::createRenderPasses() {
  // create renderpass with 2 subpasses
  RenderPassInfo info_pass{2};
  info_pass.setAttachment(0, this->m_images.at("color").format(), vk::ImageLayout::eColorAttachmentOptimal);
  info_pass.setAttachment(1, this->m_images.at("pos").format(), vk::ImageLayout::eColorAttachmentOptimal);
  info_pass.setAttachment(2, this->m_images.at("normal").format(), vk::ImageLayout::eColorAttachmentOptimal);
  info_pass.setAttachment(3, this->m_images.at("depth").format(), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  info_pass.setAttachment(4, this->m_images.at("color_2").format(), vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
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
  m_render_pass = RenderPass{this->m_device, info_pass};
}

template<typename T>
void ApplicationSimple<T>::createPipelines() {
  GraphicsPipelineInfo info_pipe;
  GraphicsPipelineInfo info_pipe2;

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

  info_pipe.setShader(this->m_shaders.at("scene"));
  info_pipe.setVertexInput(this->m_model.vertexInfo());
  info_pipe.setPass(m_render_pass, 0);
  info_pipe.addDynamic(vk::DynamicState::eViewport);
  info_pipe.addDynamic(vk::DynamicState::eScissor);

  // glm::fvec3 color{0.0f, 0.0f, 1.0f};
  // info_pipe.setSpecConstant(vk::ShaderStageFlagBits::eFragment, 0, color.r);
  // info_pipe.setSpecConstant(vk::ShaderStageFlagBits::eFragment, 1, color.g);
  // info_pipe.setSpecConstant(vk::ShaderStageFlagBits::eFragment, 2, color.b);

  vk::PipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = vk::CompareOp::eLess;
  info_pipe.setDepthStencil(depthStencil);

  info_pipe2.setResolution(this->m_swap_chain.extent());
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

  info_pipe2.setVertexInput(this->m_model.vertexInfo());
  info_pipe2.setShader(this->m_shaders.at("lights"));
  info_pipe2.setPass(m_render_pass, 1);
  info_pipe2.addDynamic(vk::DynamicState::eViewport);
  info_pipe2.addDynamic(vk::DynamicState::eScissor);

  vk::PipelineDepthStencilStateCreateInfo depthStencil2{};
  depthStencil2.depthTestEnable = VK_TRUE;
  depthStencil2.depthWriteEnable = VK_FALSE;
  depthStencil2.depthCompareOp = vk::CompareOp::eGreater;
  info_pipe2.setDepthStencil(depthStencil2);

  this->m_pipelines.emplace("scene", GraphicsPipeline{this->m_device, info_pipe, this->m_pipeline_cache});
  this->m_pipelines.emplace("lights", GraphicsPipeline{this->m_device, info_pipe2, this->m_pipeline_cache});
}

template<typename T>
void ApplicationSimple<T>::updatePipelines() {
  auto info_pipe = this->m_pipelines.at("scene").info();
  info_pipe.setShader(this->m_shaders.at("scene"));
  this->m_pipelines.at("scene").recreate(info_pipe);

  auto info_pipe2 = this->m_pipelines.at("lights").info();
  info_pipe2.setShader(this->m_shaders.at("lights"));
  this->m_pipelines.at("lights").recreate(info_pipe2);
}

template<typename T>
void ApplicationSimple<T>::createVertexBuffer() {
  vertex_data tri = geometry_loader::obj(this->m_resource_path + "models/sphere.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);
  this->m_model = Geometry{this->m_transferrer, tri};

  tri = geometry_loader::obj(this->m_resource_path + "models/sponza.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);
  m_model_2 = Geometry{this->m_transferrer, tri};
}

template<typename T>
void ApplicationSimple<T>::createLights() {
  std::srand(5);
  for (std::size_t i = 0; i < NUM_LIGHTS; ++i) {
    light_t light;
    light.position = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)} * 25.0f - 12.5f;
    light.color = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)};
    // light.radius = float(rand()) / float(RAND_MAX) * 5.0f + 5.0f * 100.0f;
    light.radius = float(rand()) / float(RAND_MAX) * 5.0f + 5.0f;
    light.intensity = float(rand()) / float(RAND_MAX);
    buff_l.lights[i] = light;
  }
  this->m_transferrer.uploadBufferData(&buff_l, this->m_buffer_views.at("light"));
}

template<typename T>
void ApplicationSimple<T>::createFramebufferAttachments() {
 auto depthFormat = findSupportedFormat(
  this->m_device.physical(),
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );
  auto extent = extent_3d(this->m_swap_chain.extent()); 
  this->m_images["depth"] = ImageRes{this->m_device, extent, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment};
  this->m_transferrer.transitionToLayout(this->m_images.at("depth"), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("depth"));

  this->m_images["color"] = ImageRes{this->m_device, extent, this->m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  this->m_transferrer.transitionToLayout(this->m_images.at("color"), vk::ImageLayout::eColorAttachmentOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("color"));

  this->m_images["pos"] = ImageRes{this->m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  this->m_transferrer.transitionToLayout(this->m_images.at("pos"), vk::ImageLayout::eColorAttachmentOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("pos"));

  this->m_images["normal"] = ImageRes{this->m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  this->m_transferrer.transitionToLayout(this->m_images.at("normal"), vk::ImageLayout::eColorAttachmentOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("normal"));

  this->m_images["color_2"] = ImageRes{this->m_device, extent, this->m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc};
  this->m_transferrer.transitionToLayout(this->m_images.at("color_2"), vk::ImageLayout::eTransferSrcOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("color_2"));
}

template<typename T>
void ApplicationSimple<T>::createTextureImage() {
  pixel_data pix_data = texture_loader::file(this->m_resource_path + "textures/test.tga");

  this->m_images["texture"] = ImageRes{this->m_device, pix_data.extent, pix_data.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst};
  this->m_allocators.at("images").allocate(this->m_images.at("texture"));
 
  this->m_transferrer.uploadImageData(pix_data.ptr(), this->m_images.at("texture"), vk::ImageLayout::eShaderReadOnlyOptimal);
}

template<typename T>
void ApplicationSimple<T>::createTextureSampler() {
  m_sampler = Sampler{this->m_device, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat};
}

template<typename T>
void ApplicationSimple<T>::updateDescriptors() {
  this->m_images.at("color").view().writeToSet(this->m_descriptor_sets.at("lighting"), 0, vk::DescriptorType::eInputAttachment);
  this->m_images.at("pos").view().writeToSet(this->m_descriptor_sets.at("lighting"), 1, vk::DescriptorType::eInputAttachment);
  this->m_images.at("normal").view().writeToSet(this->m_descriptor_sets.at("lighting"), 2, vk::DescriptorType::eInputAttachment);
  this->m_buffer_views.at("light").writeToSet(this->m_descriptor_sets.at("lighting"), 3, vk::DescriptorType::eStorageBuffer);

  this->m_buffer_views.at("uniform").writeToSet(this->m_descriptor_sets.at("matrix"), 0, vk::DescriptorType::eUniformBuffer);
  this->m_images.at("texture").view().writeToSet(this->m_descriptor_sets.at("textures"), 0, m_sampler.get());
}

template<typename T>
void ApplicationSimple<T>::createDescriptorPools() {
  DescriptorPoolInfo info_pool{};
  info_pool.reserve(this->m_shaders.at("scene"), 2);
  info_pool.reserve(this->m_shaders.at("lights"), 1, 2);

  this->m_descriptor_pool = DescriptorPool{this->m_device, info_pool};
  this->m_descriptor_sets["matrix"] = this->m_descriptor_pool.allocate(this->m_shaders.at("scene").setLayout(0));
  this->m_descriptor_sets["textures"] = this->m_descriptor_pool.allocate(this->m_shaders.at("scene").setLayout(1));
  this->m_descriptor_sets["lighting"] = this->m_descriptor_pool.allocate(this->m_shaders.at("lights").setLayout(1));
}

template<typename T>
void ApplicationSimple<T>::createUniformBuffers() {
  this->m_buffers["uniforms"] = Buffer{this->m_device, (sizeof(UniformBufferObject) + sizeof(BufferLights)) * 2, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};
  this->m_buffer_views["light"] = BufferView{sizeof(BufferLights), vk::BufferUsageFlagBits::eStorageBuffer};
  this->m_buffer_views["uniform"] = BufferView{sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer};

  this->m_allocators.at("buffers").allocate(this->m_buffers.at("uniforms"));

  this->m_buffer_views.at("light").bindTo(this->m_buffers.at("uniforms"));
  this->m_buffer_views.at("uniform").bindTo(this->m_buffers.at("uniforms"));
}

///////////////////////////// update functions ////////////////////////////////
template<typename T>
void ApplicationSimple<T>::updateView() {
  UniformBufferObject ubo{};
  ubo.view = this->m_camera.viewMatrix();
  ubo.proj = this->m_camera.projectionMatrix();
  ubo.model = glm::fmat4{1.0f};
  ubo.model = glm::scale(glm::fmat4{1.0f}, glm::fvec3{0.005f});
  ubo.normal = glm::inverseTranspose(ubo.view * ubo.model);

  this->m_transferrer.uploadBufferData(&ubo, this->m_buffer_views.at("uniform"));
}

///////////////////////////// misc functions ////////////////////////////////
// handle key input
template<typename T>
void ApplicationSimple<T>::keyCallback(int key, int scancode, int action, int mods) {
}
