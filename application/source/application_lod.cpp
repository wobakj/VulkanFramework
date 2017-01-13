#include "application_lod.hpp"

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

ApplicationLod::ApplicationLod(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window, std::vector<std::string> const& args) 
 :ApplicationThreaded{resource_path, device, chain, window, args, 3}
 ,m_pipeline{m_device, vkDestroyPipeline}
 ,m_pipeline_2{m_device, vkDestroyPipeline}
 ,m_descriptorPool{m_device, vkDestroyDescriptorPool}
 ,m_descriptorPool_2{m_device, vkDestroyDescriptorPool}
 ,m_textureSampler{m_device, vkDestroySampler}
 ,m_setting_wire{false}
 ,m_setting_transparent{false}
 ,m_setting_shaded{true}
 ,m_setting_levels{false}
 ,avg_update{0.0}
 ,avg_copy{0.0}
 ,num_updates{0.0}
 ,num_copys{0.0}
{
  std::cout << "old frame num " << m_swap_chain.numImages() - 1 << std::endl;
  cmdline::parser cmd_parse{};
  cmd_parse.add<int>("cut", 'c', "cut size in MB, 0 - fourth of leaf level size", false, 0, cmdline::range(0, 1024 * 64));
  cmd_parse.add<int>("upload", 'u', "upload size in MB, 0 - 1/16 of leaf size", false, 0, cmdline::range(0, 1500));

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
  createFrameResources();

  resize();

  m_averages["copy"] = Averager<double>{};
  m_averages["draw"] = Averager<double>{};
  m_averages["update"] = Averager<double>{};
  m_averages["sema_record"] = Averager<double>{};
  m_averages["sema_draw"] = Averager<double>{};
  m_averages["cpu_record"] = Averager<double>{};
  m_averages["cpu_draw"] = Averager<double>{};
  m_averages["gpu_draw"] = Averager<double>{};

  m_timers["update"] = Timer{};
  m_timers["sema_record"] = Timer{};
  m_timers["sema_draw"] = Timer{};
  m_timers["cpu_record"] = Timer{};
  m_timers["cpu_draw"] = Timer{};
  m_timers["fence_draw"] = Timer{};
  m_timers["gpu_draw"] = Timer{};

  startRenderThread();
}

ApplicationLod::~ApplicationLod() {
  shutDown();

  double mb_per_node = double(m_model_lod.sizeNode()) / 1024.0 / 1024.0;
  std::cout << "Average LOD update time: " << m_averages.at("update").get() << " milliseconds per node, " << m_averages.at("update").get() / mb_per_node * 10.0 << " per 10 MB"<< std::endl;
  std::cout << "Average LOD copy time: " << m_averages.at("copy").get() << " milliseconds per node, " << m_averages.at("copy").get() / mb_per_node * 10.0 << " per 10 MB"<< std::endl;
  std::cout << "Average draw time: " << m_averages.at("draw").get() << " milliseconds " << std::endl;
  
  std::cout << "Average record semaphore time: " << m_averages.at("sema_record").get() << " milliseconds " << std::endl;
  std::cout << "Average draw semaphore time: " << m_averages.at("sema_draw").get() << " milliseconds " << std::endl;

  std::cout << "Average CPU record time: " << m_averages.at("cpu_record").get() << " milliseconds " << std::endl;
  std::cout << "Average CPU draw time: " << m_averages.at("cpu_draw").get() << " milliseconds " << std::endl;
  std::cout << "Average GPU draw time: " << m_averages.at("gpu_draw").get() << " milliseconds " << std::endl;
}

FrameResource ApplicationLod::createFrameResource() {
  FrameResource res{m_device};
  createCommandBuffers(res);
  res.buffer_views["uniform"] = BufferView{sizeof(UniformBufferObject)};
  res.buffer_views.at("uniform").bindTo(m_buffers.at("uniforms"));
  res.query_pools["timers"] = QueryPool{m_device, vk::QueryType::eTimestamp, 3};
  // separate transfer
  res.command_buffers.emplace("transfer", m_device.createCommandBuffer("graphics"));
  res.semaphores.emplace("transfer", m_device->createSemaphore({}));
  res.fences.emplace("transfer", Fence{m_device, vk::FenceCreateFlagBits::eSignaled});

  return res;
}

void ApplicationLod::render() {
  m_timers.at("sema_record").start();
  m_semaphore_present.wait();
  m_averages.at("sema_record").add(m_timers.at("sema_record").durationEnd());
  m_timers.at("cpu_record").start();
  
  present();

  static uint64_t frame = 0;
  ++frame;
  // get next frame to record
  uint32_t frame_record = 0;
  frame_record = m_queue_record_frames.front();
  m_queue_record_frames.pop();

  auto& resource_record = m_frame_resources.at(frame_record);
  // read out timer values
  if (frame > m_frame_resources.size()) {
    if (resource_record.num_uploads > 0) {
      auto values = resource_record.query_pools.at("timers").getTimes();
      m_averages.at("copy").add((values[1] - values[0]) / resource_record.num_uploads);
      m_averages.at("draw").add((values[2] - values[1]));
      m_averages.at("gpu_draw").add((values[2] - values[1]));
    }
  }
  // wait for previous transfer
  resource_record.fences.at("transfer").wait();
  recordTransferBuffer(resource_record);
  submitTransfer(resource_record);
  // wait for last acquisition until acquiring again
  resource_record.fenceAcquire().wait();
  acquireImage(resource_record);
  // wait for drawing finish until rerecording
  resource_record.fenceDraw().wait();
  recordDrawBuffer(resource_record);
  // add newly recorded frame for drawing
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_draw_queue};
    m_queue_draw_frames.push(frame_record);
    m_semaphore_draw.signal();
  }
  m_averages.at("cpu_record").add(m_timers.at("cpu_record").durationEnd());
}

void ApplicationLod::draw() {
  m_timers.at("sema_draw").start();
  m_semaphore_draw.wait();
  m_averages.at("sema_draw").add(m_timers.at("sema_draw").durationEnd());
  m_timers.at("cpu_draw").start();
  // allow closing of application
  if (!m_should_draw) return;

  static std::uint64_t frame = 0;
  ++frame;
  uint32_t frame_draw = 0;
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_draw_queue};
    assert(!m_queue_draw_frames.empty());
    // get frame to draw
    frame_draw = m_queue_draw_frames.front();
    m_queue_draw_frames.pop();
  }
  auto& resource_draw = m_frame_resources.at(frame_draw);
  submitDraw(resource_draw);
  // make frame avaible for rerecording
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_present_queue};
    m_queue_present_frames.push(frame_draw);
    m_semaphore_present.signal();
  }
  m_averages.at("cpu_draw").add(m_timers.at("cpu_draw").durationEnd());
}

void ApplicationLod::submitTransfer(FrameResource& res) {
  std::vector<vk::SubmitInfo> submitInfos(1,vk::SubmitInfo{});

  submitInfos[0].setCommandBufferCount(1);
  submitInfos[0].setPCommandBuffers(&res.command_buffers.at("transfer"));

  vk::Semaphore signalSemaphores[]{res.semaphores.at("transfer")};
  submitInfos[0].signalSemaphoreCount = 1;
  submitInfos[0].pSignalSemaphores = signalSemaphores;

  res.fences.at("transfer").reset();
  m_device.getQueue("transfer").submit(submitInfos, res.fences.at("transfer"));
}

void ApplicationLod::submitDraw(FrameResource& res) {
  std::vector<vk::SubmitInfo> submitInfos(1,vk::SubmitInfo{});
  // wait on image acquisition and data transfer
  vk::Semaphore waitSemaphores[]{res.semaphoreAcquire(), res.semaphores.at("transfer")};
  vk::PipelineStageFlags waitStages[]{vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eDrawIndirect};
  submitInfos[0].setWaitSemaphoreCount(2);
  submitInfos[0].setPWaitSemaphores(waitSemaphores);
  submitInfos[0].setPWaitDstStageMask(waitStages);

  submitInfos[0].setCommandBufferCount(1);
  submitInfos[0].setPCommandBuffers(&res.command_buffers.at("draw"));

  vk::Semaphore signalSemaphores[]{res.semaphoreDraw()};
  submitInfos[0].signalSemaphoreCount = 1;
  submitInfos[0].pSignalSemaphores = signalSemaphores;

  res.fenceDraw().reset();
  m_device.getQueue("graphics").submit(submitInfos, res.fenceDraw());
}

void ApplicationLod::createCommandBuffers(FrameResource& res) {
  res.command_buffers.emplace("gbuffer", m_device.createCommandBuffer("graphics", vk::CommandBufferLevel::eSecondary));
}

void ApplicationLod::updateDescriptors(FrameResource& res) {
  res.descriptor_sets["matrix"] = m_shaders.at("lod").allocateSet(m_descriptorPool.get(), 0);
  res.buffer_views.at("uniform").writeToSet(res.descriptor_sets.at("matrix"), 0);
}

void ApplicationLod::updateCommandBuffers(FrameResource& res) {
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

void ApplicationLod::recordTransferBuffer(FrameResource& res) {
  res.command_buffers.at("transfer").reset({});

  res.command_buffers.at("transfer").begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  std::size_t curr_uploads = 0;
  // store upload num for later when reading out timers
  res.num_uploads = double(curr_uploads);
  m_timers.at("update").start();
  // upload node data
  m_model_lod.update(m_camera);
  curr_uploads = m_model_lod.numUploads();
  m_model_lod.performCopiesCommand(res.command_buffers.at("transfer"));
  m_model_lod.updateDrawCommands(res.command_buffers.at("transfer"));
  if (curr_uploads > 0) {
    m_averages.at("update").add(m_timers.at("update").durationEnd() / double(curr_uploads));
  }
  res.command_buffers.at("transfer").end();
}

void ApplicationLod::recordDrawBuffer(FrameResource& res) {

  res.command_buffers.at("draw").reset({});

  res.command_buffers.at("draw").begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
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


  res.query_pools.at("timers").reset(res.command_buffers.at("draw"));
  res.query_pools.at("timers").timestamp(res.command_buffers.at("draw"), 0, vk::PipelineStageFlagBits::eTopOfPipe);
  // res.query_pools.at("timers").timestamp(res.command_buffers.at("draw"), 0, vk::PipelineStageFlagBits::eTransfer);


  res.query_pools.at("timers").timestamp(res.command_buffers.at("draw"), 1, vk::PipelineStageFlagBits::eDrawIndirect);

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

  res.query_pools.at("timers").timestamp(res.command_buffers.at("draw"), 2, vk::PipelineStageFlagBits::eColorAttachmentOutput);
  res.command_buffers.at("draw").end();
}

void ApplicationLod::createFramebuffers() {
  m_framebuffer = FrameBuffer{m_device, {&m_images.at("color"), &m_images.at("depth")}, m_render_pass};
}

void ApplicationLod::createRenderPasses() {
  sub_pass_t pass_1({0},{},1);
  m_render_pass = RenderPass{m_device, {m_images.at("color").info(), m_images.at("depth").info()}, {pass_1}};
}

void ApplicationLod::createPipelines() {
  
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

void ApplicationLod::createVertexBuffer(std::string const& lod_path, std::size_t cut_budget, std::size_t upload_budget) {
  m_model_lod = ModelLod{m_device, lod_path, cut_budget, upload_budget};

  model_t tri = model_loader::obj(m_resource_path + "models/sphere.obj", model_t::NORMAL | model_t::TEXCOORD);
  m_model_light = Model{m_device, tri};
}

void ApplicationLod::createLights() {
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

void ApplicationLod::createMemoryPools() {
  m_device->waitIdle();

  // allocate pool for 5 32x4 fb attachments
  m_device.reallocateMemoryPool("framebuffer", m_images.at("color").memoryTypeBits(), vk::MemoryPropertyFlagBits::eDeviceLocal, m_images.at("color").size() * 5);
  
  m_images.at("depth").bindTo(m_device.memoryPool("framebuffer"));
  m_images.at("color").bindTo(m_device.memoryPool("framebuffer"));
}

void ApplicationLod::createFramebufferAttachments() {
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

void ApplicationLod::createTextureImage() {
  pixel_data pix_data = texture_loader::file(m_resource_path + "textures/test.tga");

  m_images["texture"] = m_device.createImage(pix_data.extent, pix_data.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
  // space for 14 8x3 1028 textures
  m_device.allocateMemoryPool("textures", m_images.at("texture").memoryTypeBits(), vk::MemoryPropertyFlagBits::eDeviceLocal, m_images.at("texture").size() * 16);
  m_images.at("texture").bindTo(m_device.memoryPool("textures"));
  m_images.at("texture").transitionToLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
  
  m_device.uploadImageData(pix_data.ptr(), m_images.at("texture"));
}

void ApplicationLod::createTextureSampler() {
  m_textureSampler = m_device->createSampler({{}, vk::Filter::eLinear, vk::Filter::eLinear});
}

void ApplicationLod::createDescriptorPools() {
  // descriptor sets can be allocated for each frame resource
  m_descriptorPool = m_shaders.at("lod").createPool(m_swap_chain.numImages() - 1);
  // global descriptor sets
  m_descriptorPool_2 = m_shaders.at("lod").createPool(1);
  m_descriptor_sets["lighting"] = m_shaders.at("lod").allocateSet(m_descriptorPool_2.get(), 1);

  m_model_lod.viewNodeLevels().writeToSet(m_descriptor_sets.at("lighting"), 1);
  m_images.at("texture").writeToSet(m_descriptor_sets.at("lighting"), 2, m_textureSampler.get());
  m_buffer_views.at("light").writeToSet(m_descriptor_sets.at("lighting"), 3);
}

void ApplicationLod::createUniformBuffers() {
  m_buffers["uniforms"] = Buffer{m_device, sizeof(BufferLights) * 4, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst};
  // allocate memory pool for uniforms
  m_device.allocateMemoryPool("uniforms", m_buffers.at("uniforms").memoryTypeBits(), vk::MemoryPropertyFlagBits::eDeviceLocal, m_buffers.at("uniforms").size() * 2);
  m_buffers.at("uniforms").bindTo(m_device.memoryPool("uniforms"));

  m_buffer_views["light"] = BufferView{sizeof(BufferLights)};
  m_buffer_views.at("light").bindTo(m_buffers.at("uniforms"));
}

///////////////////////////// update functions ////////////////////////////////
void ApplicationLod::updateView() {
  ubo_cam.model = glm::mat4();
  ubo_cam.view = m_camera.viewMatrix();
  ubo_cam.normal = glm::inverseTranspose(ubo_cam.view * ubo_cam.model);
  ubo_cam.proj = m_camera.projectionMatrix();
  ubo_cam.levels = m_setting_levels ? glm::fvec4{1.0f} : glm::fvec4{0.0};
  ubo_cam.shade = m_setting_shaded ? glm::fvec4{1.0f} : glm::fvec4{0.0};
}

///////////////////////////// misc functions ////////////////////////////////
// handle key input
void ApplicationLod::keyCallback(int key, int scancode, int action, int mods) {
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
  Launcher::run<ApplicationLod>(argc, argv);
}