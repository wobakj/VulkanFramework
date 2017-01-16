#include "application_lod_single.hpp"

#include "launcher.hpp"
#include "image.hpp"
#include "buffer.hpp"
#include "shader.hpp"
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

ApplicationLodSingle::ApplicationLodSingle(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window, std::vector<std::string> const& args) 
 :Application{resource_path, device, chain, window, args}
 ,m_pipeline{m_device, vkDestroyPipeline}
 ,m_pipeline_2{m_device, vkDestroyPipeline}
 ,m_descriptorPool{m_device, vkDestroyDescriptorPool}
 ,m_descriptorPool_2{m_device, vkDestroyDescriptorPool}
 ,m_textureSampler{m_device, vkDestroySampler}
 ,m_setting_wire{false}
 ,m_setting_transparent{false}
 ,m_setting_shaded{true}
 ,m_setting_levels{false}
{
  std::cout << "old frame num " << m_swap_chain.numImages() - 1 << std::endl;
  cmdline::parser cmd_parse{};
  cmd_parse.add<int>("cut", 'c', "cut size in MB, 0 - fourth of leaf level size", false, 0, cmdline::range(0, 1024 * 64));
  cmd_parse.add<int>("upload", 'u', "upload size in MB, 0 - 1/16 of leaf size", false, 0, cmdline::range(0, 1500));
  cmd_parse.add("debug", 'd', "debug with validation layers");

  cmd_parse.parse_check(args);
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

  m_shaders.emplace("lod", Shader{m_device, {m_resource_path + "shaders/simple_vert.spv", m_resource_path + "shaders/forward_lod_frag.spv"}});

  createUniformBuffers();
  createLights();  
  createTextureImage();
  createTextureSampler();

  m_statistics.addAverager("gpu_copy");
  m_statistics.addAverager("gpu_draw");

  m_statistics.addTimer("update");
  m_statistics.addTimer("render");
  m_statistics.addTimer("fence_acquire");

  m_frame_resource = createFrameResource();
  
  resize();
}

ApplicationLodSingle::~ApplicationLodSingle() {
  m_frame_resource.fenceAcquire().wait();
  m_frame_resource.fenceDraw().wait();

  double mb_per_node = double(m_model_lod.sizeNode()) / 1024.0 / 1024.0;
  std::cout << "Average LOD update time: " << m_statistics.get("update") << " milliseconds per node, " << m_statistics.get("update") / mb_per_node * 10.0 << " per 10 MB"<< std::endl;
  std::cout << "Average GPU draw time: " << m_statistics.get("gpu_draw") << " milliseconds " << std::endl;
  std::cout << "Average GPU copy time: " << m_statistics.get("gpu_copy") << " milliseconds per node, " << m_statistics.get("gpu_copy") / mb_per_node * 10.0 << " per 10 MB"<< std::endl;
  std::cout << std::endl;
  std::cout << "Average render time: " << m_statistics.get("render") << " milliseconds" << std::endl;
  std::cout << "Average acquire fence time: " << m_statistics.get("fence_acquire") << " milliseconds" << std::endl;
}

FrameResource ApplicationLodSingle::createFrameResource() {
  FrameResource res{m_device};
  res.command_buffers.emplace("gbuffer", m_device.createCommandBuffer("graphics", vk::CommandBufferLevel::eSecondary));
  res.buffer_views["uniform"] = BufferView{sizeof(UniformBufferObject)};
  res.buffer_views.at("uniform").bindTo(m_buffers.at("uniforms"));
  res.query_pools["timers"] = QueryPool{m_device, vk::QueryType::eTimestamp, 4};
  return res;
}

void ApplicationLodSingle::updateDescriptors(FrameResource& res) {
  res.descriptor_sets["matrix"] = m_shaders.at("lod").allocateSet(m_descriptorPool.get(), 0);
  res.buffer_views.at("uniform").writeToSet(res.descriptor_sets.at("matrix"), 0);
}

void ApplicationLodSingle::updateCommandBuffers(FrameResource& res) {
  res.command_buffers.at("gbuffer").reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  res.command_buffers.at("gbuffer").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});
  // res.query_pools.at("timers").timestamp(res.command_buffers.at("gbuffer"), 1, vk::PipelineStageFlagBits::eTopOfPipe);

  res.command_buffers.at("gbuffer").bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());

  res.command_buffers.at("gbuffer").bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_shaders.at("lod").pipelineLayout(), 0, {res.descriptor_sets.at("matrix"), m_descriptor_sets.at("lighting")}, {});

  res.command_buffers.at("gbuffer").bindVertexBuffers(0, {m_model_lod.buffer()}, {0});

  res.command_buffers.at("gbuffer").drawIndirect(m_model_lod.viewDrawCommands().buffer(), m_model_lod.viewDrawCommands().offset(), uint32_t(m_model_lod.numNodes()), sizeof(vk::DrawIndirectCommand));      

  res.command_buffers.at("gbuffer").end();
}

void ApplicationLodSingle::render() { 
  // make sure image was acquired
  m_statistics.start("fence_acquire");
  m_frame_resource.fenceAcquire().wait();
  m_statistics.stop("fence_acquire");
  m_statistics.start("render");
  acquireImage(m_frame_resource);
  // make sure no command buffer is in use
  m_frame_resource.fenceDraw().wait();
  static uint64_t frame = 0;
  ++frame;
  recordTransferBuffer(m_frame_resource);
  recordDrawBuffer(m_frame_resource);
  
  submitDraw(m_frame_resource);

  presentFrame(m_frame_resource);
  m_statistics.stop("render");
}

void ApplicationLodSingle::recordTransferBuffer(FrameResource& res) {
  // read out timer values from previous draw
  // if (res.num_uploads > 0) {
  //   auto values = res.query_pools.at("timers").getTimes();
  //   m_statistics.add("gpu_copy", (values[1] - values[0]) / res.num_uploads);
  //   m_statistics.add("gpu_draw", (values[3] - values[2]));
  // }
  std::size_t curr_uploads = 0;
  // store upload num for later when reading out timers
  res.num_uploads = double(curr_uploads);
  m_statistics.start("update");
  // upload node data
  m_model_lod.update(m_camera);
  curr_uploads = m_model_lod.numUploads();
  if (curr_uploads > 0) {
    m_statistics.add("update", m_statistics.stopValue("update") / double(curr_uploads));
  }
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
  
  vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

  vk::Viewport viewport{};
  viewport.width = (float) m_swap_chain.extent().width;
  viewport.height = (float) m_swap_chain.extent().height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  vk::Rect2D scissor{};
  scissor.extent = m_swap_chain.extent();

  vk::PipelineViewportStateCreateInfo viewportState{};
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

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

  vk::PipelineMultisampleStateCreateInfo multisampling{};
  // rgba additive blending
  vk::PipelineColorBlendAttachmentState colorBlendAttachment2{};
  colorBlendAttachment2.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment2.blendEnable = VK_TRUE;
  colorBlendAttachment2.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
  colorBlendAttachment2.dstColorBlendFactor = vk::BlendFactor::eDstAlpha;
  colorBlendAttachment2.colorBlendOp = vk::BlendOp::eAdd;
  colorBlendAttachment2.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment2.dstAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment2.alphaBlendOp = vk::BlendOp::eAdd;

  vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment.blendEnable = VK_FALSE;
  std::vector<vk::PipelineColorBlendAttachmentState> states{};
  if (m_setting_transparent) {
    states = {colorBlendAttachment2};
  }
  else {
    states = {colorBlendAttachment};
  }

  vk::PipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.attachmentCount = uint32_t(states.size());
  colorBlending.pAttachments = states.data();

  vk::PipelineDepthStencilStateCreateInfo depthStencil{};
  if (!m_setting_transparent) {
    depthStencil.depthTestEnable = VK_TRUE;
  }
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = vk::CompareOp::eLess;
  // VkDynamicState dynamicStates[] = {
  //   VK_DYNAMIC_STATE_VIEWPORT,
  //   VK_DYNAMIC_STATE_LINE_WIDTH
  // };

  // VkPipelineDynamicStateCreateInfo dynamicState = {};
  // dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  // dynamicState.dynamicStateCount = 2;
  // dynamicState.pDynamicStates = dynamicStates;
  auto pipelineInfo = m_shaders.at("lod").startPipelineInfo();
  // if (m_setting_levels) {
  //   pipelineInfo = m_shaders.at("simple").startPipelineInfo();
  // }

  auto vert_info = m_model_lod.inputInfo();
  pipelineInfo.pVertexInputState = &vert_info;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;  
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = nullptr; // Optional
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = nullptr; // Optional
  pipelineInfo.pDepthStencilState = &depthStencil;
  
  pipelineInfo.renderPass = m_render_pass;
  pipelineInfo.subpass = 0;

  pipelineInfo.flags = vk::PipelineCreateFlagBits::eAllowDerivatives;
  if (m_pipeline && m_pipeline_2) {
    pipelineInfo.flags |= vk::PipelineCreateFlagBits::eDerivative;
    // insert previously created pipeline here to derive this one from
    pipelineInfo.basePipelineHandle = m_pipeline.get();
    pipelineInfo.basePipelineIndex = -1; // Optional
  }
  auto pipelines = m_device->createGraphicsPipelines(vk::PipelineCache{}, {pipelineInfo});
  m_pipeline = pipelines[0];
}

void ApplicationLodSingle::createVertexBuffer(std::string const& lod_path, std::size_t cut_budget, std::size_t upload_budget) {
  m_model_lod = ModelLod{m_device, lod_path, cut_budget, upload_budget};

  model_t tri = model_loader::obj(m_resource_path + "models/sphere.obj", model_t::NORMAL | model_t::TEXCOORD);
  m_model_light = Model{m_device, tri};
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

void ApplicationLodSingle::resize() {
  m_frame_resource.fenceDraw().wait();
  createFramebufferAttachments();
  createRenderPasses();
  createFramebuffers();

  recreatePipeline();
}
void ApplicationLodSingle::recreatePipeline() {
  // make sure pipeline is free before rebuilding
  m_frame_resource.fenceDraw().wait();
  createPipelines();
  createDescriptorPools();

  updateDescriptors(m_frame_resource);
  updateCommandBuffers(m_frame_resource);
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