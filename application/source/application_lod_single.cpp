#include "application_lod_single.hpp"

#include "app/launcher.hpp"
#include "texture_loader.hpp"
#include "model_loader.hpp"
#include "cmdline.h"

// c++ warpper
#include <vulkan/vulkan.hpp>

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <iostream>

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
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

cmdline::parser ApplicationLodSingle::getParser() {
  cmdline::parser cmd_parse{};
  cmd_parse.add<int>("cut", 'c', "cut size in MB, 0 - fourth of leaf level size", false, 0, cmdline::range(0, 1024 * 64));
  cmd_parse.add<int>("upload", 'u', "upload size in MB, 0 - 1/16 of leaf size", false, 0, cmdline::range(0, 1500));
  // cmd_parse.add("debug", 'd', "debug with validation layers");
  return cmd_parse;
}
// child classes must overwrite
const uint32_t ApplicationLodSingle::imageCount = 2;

ApplicationLodSingle::ApplicationLodSingle(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window, cmdline::parser const& cmd_parse) 
 :ApplicationSingle{resource_path, device, chain, window, cmd_parse}
 ,m_descriptorPool{m_device, vkDestroyDescriptorPool}
 ,m_descriptorPool_2{m_device, vkDestroyDescriptorPool}
 ,m_textureSampler{m_device, vkDestroySampler}
 ,m_setting_wire{false}
 ,m_setting_transparent{false}
 ,m_setting_shaded{true}
 ,m_setting_levels{false}
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

  m_shaders.emplace("lod", Shader{m_device, {m_resource_path + "shaders/lod_vert.spv", m_resource_path + "shaders/forward_lod_frag.spv"}});

  createUniformBuffers();
  createLights();  
  createTextureImage();
  createTextureSampler();

  m_statistics.addAverager("gpu_copy");
  m_statistics.addAverager("gpu_draw");
  m_statistics.addAverager("uploads");

  m_statistics.addTimer("update");

  createRenderResources();
}

ApplicationLodSingle::~ApplicationLodSingle() {
  shutDown();

  double mb_per_node = double(m_model_lod.sizeNode()) / 1024.0 / 1024.0;
  std::cout << "Average upload: " << m_statistics.get("uploads") * mb_per_node << " MB"<< std::endl;
  std::cout << "Average LOD update time: " << m_statistics.get("update") << " milliseconds per node, " << m_statistics.get("update") / mb_per_node * 10.0 << " per 10 MB"<< std::endl;
  std::cout << "Average GPU draw time: " << m_statistics.get("gpu_draw") << " milliseconds " << std::endl;
  std::cout << "Average GPU copy time: " << m_statistics.get("gpu_copy") << " milliseconds per node, " << m_statistics.get("gpu_copy") / mb_per_node * 10.0 << " per 10 MB"<< std::endl;
  std::cout << std::endl;
  std::cout << "Average render time: " << m_statistics.get("render") << " milliseconds" << std::endl;
  std::cout << "Average acquire fence time: " << m_statistics.get("fence_acquire") << " milliseconds" << std::endl;
  std::cout << "Average draw fence time: " << m_statistics.get("fence_draw") << " milliseconds" << std::endl;
}

FrameResource ApplicationLodSingle::createFrameResource() {
  FrameResource res = ApplicationSingle::createFrameResource();
  res.command_buffers.emplace("gbuffer", m_device.createCommandBuffer("graphics", vk::CommandBufferLevel::eSecondary));
  res.buffer_views["uniform"] = BufferView{sizeof(UniformBufferObject)};
  res.buffer_views.at("uniform").bindTo(m_buffers.at("uniforms"));
  res.query_pools["timers"] = QueryPool{m_device, vk::QueryType::eTimestamp, 4};
  return res;
}

void ApplicationLodSingle::updateResourceDescriptors(FrameResource& res) {
  res.descriptor_sets["matrix"] = m_shaders.at("lod").allocateSet(m_descriptorPool.get(), 0);
  res.buffer_views.at("uniform").writeToSet(res.descriptor_sets.at("matrix"), 0);
}

void ApplicationLodSingle::updateResourceCommandBuffers(FrameResource& res) {
  res.command_buffers.at("gbuffer").reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  res.command_buffers.at("gbuffer").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});
  // res.query_pools.at("timers").timestamp(res.command_buffers.at("gbuffer"), 1, vk::PipelineStageFlagBits::eTopOfPipe);

  res.command_buffers.at("gbuffer").bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene"));

  res.command_buffers.at("gbuffer").bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene").layout(), 0, {res.descriptor_sets.at("matrix"), m_descriptor_sets.at("lighting")}, {});
  res.command_buffers.at("gbuffer").setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("gbuffer").setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("gbuffer").bindVertexBuffers(0, {m_model_lod.buffer()}, {0});

  res.command_buffers.at("gbuffer").drawIndirect(m_model_lod.viewDrawCommands().buffer(), m_model_lod.viewDrawCommands().offset(), uint32_t(m_model_lod.numNodes()), sizeof(vk::DrawIndirectCommand));      

  res.command_buffers.at("gbuffer").end();
}

void ApplicationLodSingle::recordTransferBuffer(FrameResource& res) {
  // read out timer values from previous draw
  if (res.num_uploads > 0.0) {
    auto values = res.query_pools.at("timers").getTimes();
    m_statistics.add("gpu_copy", (values[1] - values[0]) / res.num_uploads);
    m_statistics.add("gpu_draw", (values[3] - values[2]));
  }

  m_statistics.start("update");
  // upload node data
  m_model_lod.update(m_camera);
  std::size_t curr_uploads = m_model_lod.numUploads();
  m_statistics.add("uploads", double(curr_uploads));
  if (curr_uploads > 0) {
    m_statistics.add("update", m_statistics.stopValue("update") / double(curr_uploads));
  }
  // store upload num for later when reading out timers
  res.num_uploads = double(curr_uploads);

  // write transfer command buffer
  res.command_buffers.at("draw").reset({});

  res.command_buffers.at("draw").begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  res.query_pools.at("timers").reset(res.command_buffers.at("draw"));
  res.query_pools.at("timers").timestamp(res.command_buffers.at("draw"), 0, vk::PipelineStageFlagBits::eTopOfPipe);


  m_model_lod.performCopiesCommand(res.command_buffers.at("draw"));
  m_model_lod.updateDrawCommands(res.command_buffers.at("draw"));
  
  res.query_pools.at("timers").timestamp(res.command_buffers.at("draw"), 1, vk::PipelineStageFlagBits::eBottomOfPipe);
  // res.command_buffers.at("draw").end();
}

void ApplicationLodSingle::recordDrawBuffer(FrameResource& res) {
  recordTransferBuffer(res);
  // res.command_buffers.at("draw").reset({});

  // res.command_buffers.at("draw").begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  // always update, because last update could have been to other frame
  updateView();
  res.command_buffers.at("draw").updateBuffer(
    res.buffer_views.at("uniform").buffer(),
    res.buffer_views.at("uniform").offset(),
    sizeof(ubo_cam),
    &ubo_cam
  );
  // barrier to make new data visible to vertex shader
  vk::BufferMemoryBarrier barrier_buffer{};
  barrier_buffer.buffer = res.buffer_views.at("uniform").buffer();
  barrier_buffer.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
  barrier_buffer.dstAccessMask = vk::AccessFlagBits::eUniformRead;
  barrier_buffer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier_buffer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  res.command_buffers.at("draw").pipelineBarrier(
    vk::PipelineStageFlagBits::eTransfer,
    vk::PipelineStageFlagBits::eVertexShader,
    vk::DependencyFlags{},
    {},
    {barrier_buffer},
    {}
  );


  // res.query_pools.at("timers").reset(res.command_buffers.at("draw"));
  res.query_pools.at("timers").timestamp(res.command_buffers.at("draw"), 2, vk::PipelineStageFlagBits::eTopOfPipe);

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

  res.query_pools.at("timers").timestamp(res.command_buffers.at("draw"), 3, vk::PipelineStageFlagBits::eColorAttachmentOutput);
  res.command_buffers.at("draw").end();
}

void ApplicationLodSingle::createFramebuffers() {
  m_framebuffer = FrameBuffer{m_device, {&m_images.at("color"), &m_images.at("depth")}, m_render_pass};
}

void ApplicationLodSingle::createRenderPasses() {
  sub_pass_t pass_1({0},{},1);
  m_render_pass = RenderPass{m_device, {m_images.at("color").info(), m_images.at("depth").info()}, {pass_1}};
}

void ApplicationLodSingle::createPipelines() {
  PipelineInfo info_pipe;
  PipelineInfo info_pipe2;

  info_pipe.setResolution(m_swap_chain.extent());
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

  info_pipe.setShader(m_shaders.at("lod"));
  info_pipe.setVertexInput(m_model_lod);
  info_pipe.setPass(m_render_pass, 0);
  info_pipe.addDynamic(vk::DynamicState::eViewport);
  info_pipe.addDynamic(vk::DynamicState::eScissor);

  m_pipelines.emplace("scene", Pipeline{m_device, info_pipe, m_pipeline_cache});
}

void ApplicationLodSingle::updatePipelines() {
  auto info_pipe = m_pipelines.at("scene").info();

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

  info_pipe.setShader(m_shaders.at("lod"));
  m_pipelines.at("scene").recreate(info_pipe);
}

void ApplicationLodSingle::createVertexBuffer(std::string const& lod_path, std::size_t cut_budget, std::size_t upload_budget) {
  m_model_lod = GeometryLod{m_device, lod_path, cut_budget, upload_budget};

  vertex_data tri = model_loader::obj(m_resource_path + "models/sphere.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);
  m_model_light = Geometry{m_device, tri};
}

void ApplicationLodSingle::createLights() {
  std::srand(5);
  for (std::size_t i = 0; i < NUM_LIGHTS; ++i) {
    light_t light;
    light.position = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)} * 10.0f - 5.0f;
    light.color = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)};
    light.radius = float(rand()) / float(RAND_MAX) * 2.5f + 2.5f;
    buff_l.lights[i] = light;
  }
  m_device.uploadBufferData(&buff_l, m_buffer_views.at("light"));
}

void ApplicationLodSingle::createMemoryPools() {
  m_device->waitIdle();

  // allocate pool for 5 32x4 fb attachments
  m_device.reallocateMemoryPool("framebuffer", m_images.at("color").memoryTypeBits(), vk::MemoryPropertyFlagBits::eDeviceLocal, m_images.at("color").size() * 5);
  
  m_images.at("depth").bindTo(m_device.memoryPool("framebuffer"));
  m_images.at("color").bindTo(m_device.memoryPool("framebuffer"));
}

void ApplicationLodSingle::createFramebufferAttachments() {
 auto depthFormat = findSupportedFormat(
  m_device.physical(),
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );
  auto extent = vk::Extent3D{m_swap_chain.extent().width, m_swap_chain.extent().height, 1}; 
  m_images["depth"] = m_device.createImage(extent, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment);
  m_images.at("depth").transitionToLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

  m_images["color"] = m_device.createImage(extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
  m_images.at("color").transitionToLayout(vk::ImageLayout::eTransferSrcOptimal);

  createMemoryPools();
}

void ApplicationLodSingle::createTextureImage() {
  pixel_data pix_data = texture_loader::file(m_resource_path + "textures/test.tga");

  m_images["texture"] = m_device.createImage(pix_data.extent, pix_data.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
  // space for 14 8x3 1028 textures
  m_device.allocateMemoryPool("textures", m_images.at("texture").memoryTypeBits(), vk::MemoryPropertyFlagBits::eDeviceLocal, m_images.at("texture").size() * 16);
  m_images.at("texture").bindTo(m_device.memoryPool("textures"));
  m_images.at("texture").transitionToLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
  
  m_device.uploadImageData(pix_data.ptr(), m_images.at("texture"));
}

void ApplicationLodSingle::createTextureSampler() {
  m_textureSampler = m_device->createSampler({{}, vk::Filter::eLinear, vk::Filter::eLinear});
}

void ApplicationLodSingle::createDescriptorPools() {
  // descriptor sets can be allocated for each frame resource
  m_descriptorPool = m_shaders.at("lod").createPool(m_swap_chain.numImages() - 1);
  // global descriptor sets
  m_descriptorPool_2 = m_shaders.at("lod").createPool(1);
  m_descriptor_sets["lighting"] = m_shaders.at("lod").allocateSet(m_descriptorPool_2.get(), 1);

  m_model_lod.viewNodeLevels().writeToSet(m_descriptor_sets.at("lighting"), 1);
  m_images.at("texture").writeToSet(m_descriptor_sets.at("lighting"), 2, m_textureSampler.get());
  m_buffer_views.at("light").writeToSet(m_descriptor_sets.at("lighting"), 3);
}

void ApplicationLodSingle::createUniformBuffers() {
  m_buffers["uniforms"] = Buffer{m_device, sizeof(BufferLights) * 4, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst};
  // allocate memory pool for uniforms
  m_device.allocateMemoryPool("uniforms", m_buffers.at("uniforms").memoryTypeBits(), vk::MemoryPropertyFlagBits::eDeviceLocal, m_buffers.at("uniforms").size() * 2);
  m_buffers.at("uniforms").bindTo(m_device.memoryPool("uniforms"));

  m_buffer_views["light"] = BufferView{sizeof(BufferLights)};
  m_buffer_views.at("light").bindTo(m_buffers.at("uniforms"));
}

///////////////////////////// update functions ////////////////////////////////
void ApplicationLodSingle::updateView() {
  ubo_cam.model = glm::mat4();
  ubo_cam.view = m_camera.viewMatrix();
  ubo_cam.normal = glm::inverseTranspose(ubo_cam.view * ubo_cam.model);
  ubo_cam.proj = m_camera.projectionMatrix();
  ubo_cam.levels = m_setting_levels ? glm::fvec4{1.0f} : glm::fvec4{0.0};
  ubo_cam.shade = m_setting_shaded ? glm::fvec4{1.0f} : glm::fvec4{0.0};
}

///////////////////////////// misc functions ////////////////////////////////
// handle key input
void ApplicationLodSingle::keyCallback(int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
    m_setting_levels = !m_setting_levels;
    recreatePipeline();
  }
  else if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
    m_setting_wire = !m_setting_wire;
    recreatePipeline();
  }
  else if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
    m_setting_transparent = !m_setting_transparent;
    recreatePipeline();
  }
  else if (key == GLFW_KEY_4 && action == GLFW_PRESS) {
    m_setting_shaded = !m_setting_shaded;
    recreatePipeline();
  }
}

// exe entry point
int main(int argc, char* argv[]) {
  Launcher::run<ApplicationLodSingle>(argc, argv);
}