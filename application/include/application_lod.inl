#include "wrap/buffer.hpp"

#include "wrap/fence.hpp"
#include "wrap/descriptor_pool_info.hpp"
#include "wrap/conversions.hpp"
#include "wrap/descriptor_pool.hpp"
#include "wrap/image.hpp"

#include "texture_loader.hpp"
#include "geometry_loader.hpp"

#include "cmdline.h"

// c++ warpper
#include <vulkan/vulkan.hpp>

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>

struct UniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 model;
    glm::mat4 normal;
    glm::fvec4 levels;
    glm::fvec4 shade;
} ubo_cam;

struct light_t {
  glm::fvec3 position;
  float pad = 0.0f;
  glm::fvec3 color;
  float radius;
};
const std::size_t NUM_LIGHTS = 60;
struct BufferLights {
  light_t lights[NUM_LIGHTS];
};
BufferLights buff_l;

template<typename T>
cmdline::parser ApplicationLod<T>::getParser() {
  cmdline::parser cmd_parse{T::getParser()};
  cmd_parse.add<int>("cut", 'c', "cut size in MB, 0 - fourth of leaf level size", false, 0, cmdline::range(0, 1024 * 64));
  cmd_parse.add<int>("upload", 'u', "upload size in MB, 0 - 1/16 of leaf size", false, 0, cmdline::range(0, 1500));
  return cmd_parse;
}

template<typename T>
ApplicationLod<T>::ApplicationLod(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :T{resource_path, device, surf, cmd_parse}
 ,m_setting_wire{false}
 ,m_setting_transparent{false}
 ,m_setting_shaded{true}
 ,m_setting_levels{true}
{
  if (cmd_parse.rest().size() != 1) {
    if (cmd_parse.rest().size() < 1) {
      std::cerr << "No filename specified" << std::endl;
    }
    else {
      std::cerr << cmd_parse.usage();
    }
    exit(0);
  }

  createVertexBuffer(cmd_parse.rest()[0], cmd_parse.get<int>("cut"), cmd_parse.get<int>("upload"));

  this->m_shaders.emplace("lod", Shader{this->m_device, {this->m_resource_path + "shaders/lod_vert.spv", this->m_resource_path + "shaders/forward_lod_frag.spv"}});

  createUniformBuffers();
  createLights();  
  createTextureImage();
  createTextureSampler();

  this->createRenderResources();

  this->m_statistics.addAverager("gpu_copy");
  this->m_statistics.addAverager("gpu_draw");
  this->m_statistics.addAverager("uploads");

  this->m_statistics.addTimer("update");
}

template<typename T>
ApplicationLod<T>::~ApplicationLod() {
  this->shutDown();

  double mb_per_node = double(m_model_lod.sizeNode()) / 1024.0 / 1024.0;
  std::cout << "Average upload: " << this->m_statistics.get("uploads") * mb_per_node << " MB"<< std::endl;
  std::cout << "Average LOD update time: " << this->m_statistics.get("update") << " milliseconds per node, " << this->m_statistics.get("update") / mb_per_node * 10.0 << " per 10 MB"<< std::endl;
  std::cout << "Average GPU draw time: " << this->m_statistics.get("gpu_draw") << " milliseconds " << std::endl;
  std::cout << "Average GPU copy time: " << this->m_statistics.get("gpu_copy") << " milliseconds per node, " << this->m_statistics.get("gpu_copy") / mb_per_node * 10.0 << " per 10 MB"<< std::endl;
}

template<typename T>
FrameResource ApplicationLod<T>::createFrameResource() {
  auto res = T::createFrameResource();
  res.setCommandBuffer("gbuffer", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.buffer_views["uniform"] = BufferView{sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer};
  res.buffer_views.at("uniform").bindTo(this->m_buffers.at("uniforms"));
  res.query_pools["timers"] = QueryPool{this->m_device, vk::QueryType::eTimestamp, 4};
  return res;
}

template<typename T>
void ApplicationLod<T>::updateResourceDescriptors(FrameResource& res) {
  res.buffer_views.at("uniform").writeToSet(res.descriptor_sets.at("matrix"), 0, vk::DescriptorType::eUniformBuffer);
}

template<typename T>
void ApplicationLod<T>::updateResourceCommandBuffers(FrameResource& res) {
  res.commandBuffer("gbuffer")->reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  res.commandBuffer("gbuffer")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});
  // res.query_pools.at("timers").timestamp(res.commandBuffer("gbuffer"), 1, vk::PipelineStageFlagBits::eTopOfPipe);

  res.commandBuffer("gbuffer")->bindPipeline(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("scene"));

  res.commandBuffer("gbuffer")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("scene").layout(), 0, {res.descriptor_sets.at("matrix"), this->m_descriptor_sets.at("lighting")}, {});

  res.commandBuffer("gbuffer")->setViewport(0, viewport(this->m_resolution));
  res.commandBuffer("gbuffer")->setScissor(0, rect(this->m_resolution));

  res.commandBuffer("gbuffer")->bindVertexBuffers(0, {m_model_lod.buffer()}, {0});

  res.commandBuffer("gbuffer")->drawIndirect(m_model_lod.viewDrawCommands().buffer(), m_model_lod.viewDrawCommands().offset(), uint32_t(m_model_lod.numNodes()), sizeof(vk::DrawIndirectCommand));      

  res.commandBuffer("gbuffer")->end();
}

template<typename T>
void ApplicationLod<T>::recordTransferBuffer(FrameResource& res) {
  // read out timer values from previous draw
  // if (res.num_uploads > 0.0) {
  //   auto values = res.query_pools.at("timers").getTimes();
  //   this->m_statistics.add("gpu_copy", (values[1] - values[0]) / res.num_uploads);
  //   this->m_statistics.add("gpu_draw", (values[3] - values[2]));
  // }

  this->m_statistics.start("update");
  // upload node data
  m_model_lod.update(this->matrixView(), this->matrixFrustum());
  size_t curr_uploads = m_model_lod.numUploads();
  this->m_statistics.add("uploads", double(curr_uploads));
  if (curr_uploads > 0) {
    this->m_statistics.add("update", this->m_statistics.stopValue("update") / double(curr_uploads));
  }
  // store upload num for later when reading out timers
  res.num_uploads = double(curr_uploads);

  // write transfer command buffer
  res.commandBuffer("transfer")->reset({});

  res.commandBuffer("transfer")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  // res.query_pools.at("timers").reset(res.commandBuffer("transfer"));
  // res.query_pools.at("timers").timestamp(res.commandBuffer("transfer"), 0, vk::PipelineStageFlagBits::eTopOfPipe);

  m_model_lod.performCopiesCommand(res.commandBuffer("transfer"));
  m_model_lod.updateDrawCommands(res.commandBuffer("transfer"));
  
  // res.query_pools.at("timers").timestamp(res.commandBuffer("transfer"), 1, vk::PipelineStageFlagBits::eBottomOfPipe);
  res.commandBuffer("transfer")->end();
}

template<typename T>
void ApplicationLod<T>::recordDrawBuffer(FrameResource& res) {

  res.commandBuffer("draw")->reset({});

  res.commandBuffer("draw")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  // always update, because last update could have been to other frame
  updateView();
  res.commandBuffer("draw")->updateBuffer(
    res.buffer_views.at("uniform").buffer(),
    res.buffer_views.at("uniform").offset(),
    sizeof(ubo_cam),
    &ubo_cam
  );
  // make matrices visible to vertex shader
  res.command_buffers.at("draw").bufferBarrier(res.buffer_views.at("uniform").buffer(), 
    vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite, 
    vk::PipelineStageFlagBits::eVertexShader, vk::AccessFlagBits::eUniformRead
  );

  res.query_pools.at("timers").timestamp(res.commandBuffer("draw"), 2, vk::PipelineStageFlagBits::eTopOfPipe);

  // make draw commands visible to drawindirect
  res.command_buffers.at("draw").bufferBarrier(m_model_lod.viewDrawCommands().buffer(), 
    vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite, 
    vk::PipelineStageFlagBits::eDrawIndirect, vk::AccessFlagBits::eIndirectCommandRead
  );

  // make node data visible to vertex shader
  res.command_buffers.at("draw").bufferBarrier(m_model_lod.buffer(), 
    vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite, 
    vk::PipelineStageFlagBits::eVertexInput, vk::AccessFlagBits::eVertexAttributeRead
  );

  // make node level data visible to fragment shader
  res.command_buffers.at("draw").bufferBarrier(m_model_lod.viewNodeLevels().buffer(), 
    vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite, 
    vk::PipelineStageFlagBits::eFragmentShader, vk::AccessFlagBits::eShaderRead
  );

  res.commandBuffer("draw")->beginRenderPass(m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.commandBuffer("draw")->executeCommands({res.commandBuffer("gbuffer")});

  res.commandBuffer("draw")->endRenderPass();

  this->presentCommands(res, this->m_images.at("color").view(), vk::ImageLayout::eTransferSrcOptimal);

  res.query_pools.at("timers").timestamp(res.commandBuffer("draw"), 3, vk::PipelineStageFlagBits::eBottomOfPipe);
  res.commandBuffer("draw")->end();
}

template<typename T>
void ApplicationLod<T>::createFramebuffers() {
  m_framebuffer = FrameBuffer{this->m_device, {&this->m_images.at("color"), &this->m_images.at("depth")}, m_render_pass};
}

template<typename T>
void ApplicationLod<T>::createRenderPasses() {
  // create renderpass with 1 subpasses
  RenderPassInfo info_pass{};
  info_pass.setAttachment(0, this->m_images.at("color").format(), vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
  info_pass.setAttachment(1, this->m_images.at("depth").format(), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  info_pass.subPass(0).setColorAttachment(0, 0);
  info_pass.subPass(0).setDepthAttachment(1);
  m_render_pass = RenderPass{this->m_device, info_pass};
}

template<typename T>
void ApplicationLod<T>::createPipelines() {
  GraphicsPipelineInfo info_pipe;
  GraphicsPipelineInfo info_pipe2;
  // overwritten during recording
  info_pipe.setResolution(extent_2d(this->m_resolution));
  info_pipe.setTopology(vk::PrimitiveTopology::eTriangleList);
  
  vk::PipelineRasterizationStateCreateInfo rasterizer{};
  if (m_setting_wire) {
    rasterizer.lineWidth = 0.5f;
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    rasterizer.polygonMode = vk::PolygonMode::eLine;
  }
  else {
    rasterizer.lineWidth = 1.0f;
    if (!m_setting_transparent) {
      rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    }
  }
  info_pipe.setRasterizer(rasterizer);

  vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
  colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eDstAlpha;
  colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
  colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

  if (!m_setting_transparent) {
    colorBlendAttachment.blendEnable = VK_FALSE;
  }
  info_pipe.setAttachmentBlending(colorBlendAttachment, 0);

  vk::PipelineDepthStencilStateCreateInfo depthStencil{};
  if (!m_setting_transparent) {
    depthStencil.depthTestEnable = VK_TRUE;
  }
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = vk::CompareOp::eLess;
  info_pipe.setDepthStencil(depthStencil);

  info_pipe.setShader(this->m_shaders.at("lod"));
  info_pipe.setVertexInput(m_model_lod.vertexInfo());
  info_pipe.setPass(m_render_pass, 0);
  info_pipe.addDynamic(vk::DynamicState::eViewport);
  info_pipe.addDynamic(vk::DynamicState::eScissor);

  this->m_pipelines.emplace("scene", GraphicsPipeline{this->m_device, info_pipe, this->m_pipeline_cache});
}

template<typename T>
void ApplicationLod<T>::updatePipelines() {
  auto info_pipe = this->m_pipelines.at("scene").info();

  vk::PipelineRasterizationStateCreateInfo rasterizer{};
  if (m_setting_wire) {
    rasterizer.lineWidth = 0.5f;
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    rasterizer.polygonMode = vk::PolygonMode::eLine;
  }
  else {
    rasterizer.lineWidth = 1.0f;
    if (!m_setting_transparent) {
      rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    }
  }
  info_pipe.setRasterizer(rasterizer);

  vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
  colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eDstAlpha;
  colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
  colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

  if (!m_setting_transparent) {
    colorBlendAttachment.blendEnable = VK_FALSE;
  }
  info_pipe.setAttachmentBlending(colorBlendAttachment, 0);

  vk::PipelineDepthStencilStateCreateInfo depthStencil{};
  if (!m_setting_transparent) {
    depthStencil.depthTestEnable = VK_TRUE;
  }
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = vk::CompareOp::eLess;
  info_pipe.setDepthStencil(depthStencil);

  info_pipe.setShader(this->m_shaders.at("lod"));
  this->m_pipelines.at("scene").recreate(info_pipe);
}

template<typename T>
void ApplicationLod<T>::createVertexBuffer(std::string const& lod_path, std::size_t cut_budget, std::size_t upload_budget) {
  m_model_lod = GeometryLod{this->m_transferrer, lod_path, cut_budget, upload_budget};

  vertex_data tri = geometry_loader::obj(this->m_resource_path + "models/sphere.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);
  m_model_light = Geometry{this->m_transferrer, tri};
}

template<typename T>
void ApplicationLod<T>::createLights() {
  std::srand(5);
  for (std::size_t i = 0; i < NUM_LIGHTS; ++i) {
    light_t light;
    light.position = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)} * 10.0f - 5.0f;
    light.color = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)};
    light.radius = float(rand()) / float(RAND_MAX) * 2.5f + 2.5f;
    buff_l.lights[i] = light;
  }
  this->m_transferrer.uploadBufferData(&buff_l, this->m_buffer_views.at("light"));
}

template<typename T>
void ApplicationLod<T>::createFramebufferAttachments() {
 auto depthFormat = findSupportedFormat(
  this->m_device.physical(),
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );
  auto extent = extent_3d(this->m_resolution);
  this->m_images["depth"] = ImageRes{this->m_device, extent, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment};
  this->m_transferrer.transitionToLayout(this->m_images.at("depth"), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("depth"));

  this->m_images["color"] = ImageRes{this->m_device, extent, vk::Format::eB8G8R8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc};
  this->m_transferrer.transitionToLayout(this->m_images.at("color"), vk::ImageLayout::eTransferSrcOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("color"));
}

template<typename T>
void ApplicationLod<T>::createTextureImage() {
  pixel_data pix_data = texture_loader::file(this->m_resource_path + "textures/test.tga");

  this->m_images["texture"] = ImageRes{this->m_device, pix_data.extent, pix_data.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst};
  this->m_allocators.at("images").allocate(this->m_images.at("texture"));
  
  this->m_transferrer.uploadImageData(pix_data.ptr(), pix_data.size(), this->m_images.at("texture"), vk::ImageLayout::eShaderReadOnlyOptimal);
}

template<typename T>
void ApplicationLod<T>::createTextureSampler() {
  m_sampler = Sampler{this->m_device, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat};
}

template<typename T>
void ApplicationLod<T>::updateDescriptors() {
  m_model_lod.viewNodeLevels().writeToSet(this->m_descriptor_sets.at("lighting"), 1, vk::DescriptorType::eStorageBuffer);
  this->m_images.at("texture").view().writeToSet(this->m_descriptor_sets.at("lighting"), 2, m_sampler.get());
  this->m_buffer_views.at("light").writeToSet(this->m_descriptor_sets.at("lighting"), 3, vk::DescriptorType::eStorageBuffer);
}

template<typename T>
void ApplicationLod<T>::createDescriptorPools() {
  DescriptorPoolInfo info_pool{};
  info_pool.reserve(this->m_shaders.at("lod"), 0, uint32_t(this->m_frame_resources.size()));
  info_pool.reserve(this->m_shaders.at("lod"), 1, 1);

  this->m_descriptor_pool = DescriptorPool{this->m_device, info_pool};

  this->m_descriptor_sets["lighting"] = this->m_descriptor_pool.allocate(this->m_shaders.at("lod").setLayout(1));
  for(auto& res : this->m_frame_resources) {
    res.descriptor_sets["matrix"] = this->m_descriptor_pool.allocate(this->m_shaders.at("lod").setLayout(0));
  }
}

template<typename T>
void ApplicationLod<T>::createUniformBuffers() {
  this->m_buffers["uniforms"] = Buffer{this->m_device, sizeof(BufferLights) * 4, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};
  this->m_allocators.at("buffers").allocate(this->m_buffers.at("uniforms"));

  this->m_buffer_views["light"] = BufferView{sizeof(BufferLights), vk::BufferUsageFlagBits::eStorageBuffer};
  this->m_buffer_views.at("light").bindTo(this->m_buffers.at("uniforms"));
}


template<typename T>///////////////////////////// update functions ////////////////////////////////
void ApplicationLod<T>::updateView() {
  ubo_cam.model = glm::mat4();
  ubo_cam.view = this->matrixView();
  ubo_cam.normal = glm::inverseTranspose(ubo_cam.view * ubo_cam.model);
  ubo_cam.proj = this->matrixFrustum();
  ubo_cam.levels = m_setting_levels ? glm::fvec4{1.0f} : glm::fvec4{0.0};
  ubo_cam.shade = m_setting_shaded ? glm::fvec4{1.0f} : glm::fvec4{0.0};
}

///////////////////////////// misc functions ////////////////////////////////

template<typename T>// handle key input
void ApplicationLod<T>::keyCallback(int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
    m_setting_levels = !m_setting_levels;
    this->recreatePipeline();
  }
  else if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
    m_setting_wire = !m_setting_wire;
    this->recreatePipeline();
  }
  else if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
    m_setting_transparent = !m_setting_transparent;
    this->recreatePipeline();
  }
  else if (key == GLFW_KEY_4 && action == GLFW_PRESS) {
    m_setting_shaded = !m_setting_shaded;
    this->recreatePipeline();
  }
}