#include "application_lod.hpp"

#include "launcher.hpp"
#include "image.hpp"
#include "buffer.hpp"
#include "shader.hpp"
#include "texture_loader.hpp"
#include "model_loader.hpp"

// c++ warpper
#include <vulkan/vulkan.hpp>

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <iostream>
#include <chrono>

#define THREADING
// helper functions
std::string resourcePath(int argc, char* argv[]);
void glfw_error(int error, const char* description);

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 normal;
} ubo_cam;

struct light_t {
  glm::fvec3 position;
  float pad;
  glm::fvec3 color;
  float radius;
};
const std::size_t NUM_LIGHTS = 60;
struct BufferLights {
  light_t lights[NUM_LIGHTS];
};
BufferLights buff_l;

const std::size_t NUM_NODES = 512;

ApplicationLod::ApplicationLod(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window) 
 :Application{resource_path, device, chain, window}
 ,m_pipeline{m_device, vkDestroyPipeline}
 ,m_pipeline_2{m_device, vkDestroyPipeline}
 ,m_descriptorPool{m_device, vkDestroyDescriptorPool}
 ,m_descriptorPool_2{m_device, vkDestroyDescriptorPool}
 ,m_textureSampler{m_device, vkDestroySampler}
 ,m_should_draw{true}
 ,m_semaphore_draw{0}
 ,m_semaphore_record{m_swap_chain.numImages() - 1}
{
  m_shaders.emplace("simple", Shader{m_device, {"../resources/shaders/simple_vert.spv", "../resources/shaders/simple_frag.spv"}});
  m_shaders.emplace("quad", Shader{m_device, {"../resources/shaders/quad_vert.spv", "../resources/shaders/quad_frag.spv"}});

  createVertexBuffer();
  createUniformBuffers();
  createLights();  
  createTextureImage();
  createTextureSampler();
  createDepthResource();
  createRenderPass();

  createFrameResources();

  resize();
  render();

  if (!m_thread_render.joinable()) {
    m_thread_render = std::thread(&ApplicationLod::drawLoop, this);
  }
}

ApplicationLod::~ApplicationLod() {
  // shut down render thread
  m_should_draw = false;
  if(m_thread_render.joinable()) {
    m_thread_render.join();
  }
  else {
    throw std::runtime_error{"could not join thread"};
  }
  // wait until fences are done
  for (auto const& res : m_frame_resources) {
    res.waitFences();
  }
}

void ApplicationLod::createFrameResources() {
  // create resources for one less image than swap chain
  // only numImages - 1 images can be acquired at a time
  for (uint32_t i = 0; i < m_swap_chain.numImages() - 1; ++i) {
    m_frame_resources.emplace_back(m_device);
    auto& res = m_frame_resources.back();
    createCommandBuffers(res);
    m_queue_record_frames.push(i);
    res.buffer_views["uniform"] = BufferView{sizeof(UniformBufferObject)};
    res.buffer_views.at("uniform").bindTo(m_buffers.at("uniforms"));
  }
}

void ApplicationLod::render() {
  m_semaphore_record.wait();
  static uint64_t frame = 0;
  ++frame;
  // only calculate new frame if previous one was rendered
  if (!m_mutex_record_queue.try_lock()) return;
  if (m_queue_record_frames.empty()) {
    m_mutex_record_queue.unlock();
    return;
  }
  // get next frame to record
  uint32_t frame_record = m_queue_record_frames.front();
  m_queue_record_frames.pop();
  m_mutex_record_queue.unlock();
  auto& resource_record = m_frame_resources.at(frame_record);

  // wait for last acquisition until acquiring again
  resource_record.fenceAcquire().wait();
  resource_record.fenceAcquire().reset();
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
  // std::this_thread::sleep_for(std::chrono::milliseconds{10}); 
}

void ApplicationLod::draw() {
  m_semaphore_draw.wait();
  static std::uint64_t frame = 0;
  ++frame;
  uint32_t frame_draw = 0;
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_draw_queue};
    if (m_queue_draw_frames.empty()) {
      return;
    }
    // get frame to draw
    frame_draw = m_queue_draw_frames.front();
    m_queue_draw_frames.pop();
  }
  auto& resource_draw = m_frame_resources.at(frame_draw);
  // draw only when new frame is avaible
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_swapchain};
    // wait until drawing with these resources is finished before issuing next draw
    resource_draw.fenceDraw().wait();
    resource_draw.fenceDraw().reset();
    submitDraw(resource_draw);
    // wait until swap chain is avaible
      // present image and wait for result
      present(resource_draw);
    // make frame avaible for rerecording
    {
      std::lock_guard<std::mutex> queue_lock{m_mutex_record_queue};
      m_queue_record_frames.push(frame_draw);
      m_semaphore_record.signal();
    }
  }
}

void ApplicationLod::drawLoop() {
  while (m_should_draw) {
    draw();
  }
}

void ApplicationLod::createCommandBuffers(FrameResource& res) {
  res.command_buffers.emplace("gbuffer", m_device.createCommandBuffer("graphics", vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("lighting", m_device.createCommandBuffer("graphics", vk::CommandBufferLevel::eSecondary));
}

void ApplicationLod::updateDescriptors(FrameResource& res) {
  res.descriptor_sets["matrix"] = m_shaders.at("simple").allocateSet(m_descriptorPool.get(), 0);
  res.buffer_views.at("uniform").writeToSet(res.descriptor_sets.at("matrix"), 0);
}

void ApplicationLod::updateCommandBuffers(FrameResource& res) {
  res.command_buffers.at("gbuffer").reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  // first pass
  res.command_buffers.at("gbuffer").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("gbuffer").bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
  res.command_buffers.at("gbuffer").bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_shaders.at("simple").pipelineLayout(), 0, {res.descriptor_sets.at("matrix"), m_descriptor_sets.at("textures")}, {});

  res.command_buffers.at("gbuffer").bindVertexBuffers(0, {m_model_lod.buffer()}, {0});
  // for(std::size_t i = 0; i < m_model_lod.numNodes(); ++i) {
    res.command_buffers.at("gbuffer").drawIndirect(m_model_lod.viewDrawCommands().buffer(), m_model_lod.viewDrawCommands().offset(), uint32_t(m_model_lod.numNodes()), sizeof(vk::DrawIndirectCommand));      
  // }

  res.command_buffers.at("gbuffer").end();
  //deferred shading pass 
  inheritanceInfo.subpass = 1;
  res.command_buffers.at("lighting").reset({});
  res.command_buffers.at("lighting").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("lighting").bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline_2.get());
  res.command_buffers.at("lighting").bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_shaders.at("quad").pipelineLayout(), 0, {res.descriptor_sets.at("matrix"), m_descriptor_sets.at("lighting")}, {});

  // res.command_buffers.at("lighting").bindVertexBuffers(0, {m_model_light.buffer()}, {0});
  // res.command_buffers.at("lighting").bindIndexBuffer(m_model_light.buffer(), m_model_light.indexOffset(), vk::IndexType::eUint32);

  // res.command_buffers.at("lighting").drawIndexed(m_model_light.numIndices(), NUM_LIGHTS, 0, 0, 0);
  res.command_buffers.at("lighting").draw(4, 1, 0, 0);

  res.command_buffers.at("lighting").end();
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

  // upload node data
  m_model_lod.update(m_camera);
  m_model_lod.performCopiesCommand(res.command_buffers.at("draw"));
  m_model_lod.updateDrawCommands(res.command_buffers.at("draw"));

  res.command_buffers.at("draw").beginRenderPass(m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.command_buffers.at("draw").executeCommands({res.command_buffers.at("gbuffer")});
  
  res.command_buffers.at("draw").nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
  // execute lighting buffer
  res.command_buffers.at("draw").executeCommands({res.command_buffers.at("lighting")});

  res.command_buffers.at("draw").endRenderPass();
  // make sure rendering to image is done before blitting
  // barrier is now performed through renderpass dependency

  vk::ImageBlit blit{};
  blit.srcSubresource = img_to_resource_layer(m_images.at("color_2").info());
  blit.dstSubresource = img_to_resource_layer(m_swap_chain.imgInfo());
  blit.srcOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};
  blit.dstOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};
  res.command_buffers.at("draw").blitImage(m_images.at("color_2"), m_images.at("color_2").layout(), m_swap_chain.images().at(res.image), m_swap_chain.layout(), {blit}, vk::Filter::eNearest);

  res.command_buffers.at("draw").end();
}

void ApplicationLod::createFramebuffers() {
  m_framebuffer = FrameBuffer{m_device, {&m_images.at("color"), &m_images.at("pos"), &m_images.at("normal"), &m_images.at("depth"), &m_images.at("color_2")}, m_render_pass};
}

void ApplicationLod::createRenderPass() {
  //first pass receives attachment 0,1,2 as color, position and normal attachment and attachment 3 as depth attachments 
  sub_pass_t pass_1({0, 1, 2},{},3);
  // second pass receives attachments 0,1,2 and inputs and writes to 4
  sub_pass_t pass_2({4},{0,1,2}, 3);
  m_render_pass = RenderPass{m_device, {m_images.at("color").info(), m_images.at("pos").info(), m_images.at("normal").info(), m_images.at("depth").info(), m_images.at("color_2").info()}, {pass_1, pass_2}};
}

void ApplicationLod::createGraphicsPipeline() {
  
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
  rasterizer.lineWidth = 0.5f;
  rasterizer.cullMode = vk::CullModeFlagBits::eNone;
  // rasterizer.cullMode = vk::CullModeFlagBits::eBack;
  rasterizer.polygonMode = vk::PolygonMode::eLine;

  vk::PipelineMultisampleStateCreateInfo multisampling{};

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
  std::vector<vk::PipelineColorBlendAttachmentState> states{colorBlendAttachment2, colorBlendAttachment, colorBlendAttachment};

  vk::PipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.attachmentCount = uint32_t(states.size());
  colorBlending.pAttachments = states.data();

  vk::PipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.depthTestEnable = VK_TRUE;
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
  auto pipelineInfo = m_shaders.at("simple").startPipelineInfo();

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

  auto pipelineInfo2 = m_shaders.at("quad").startPipelineInfo();
  
  vk::PipelineVertexInputStateCreateInfo vert_info2{};
  pipelineInfo2.pVertexInputState = &vert_info2;
  // pipelineInfo2.pVertexInputState = &vert_info;
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly2{};
  inputAssembly2.topology = vk::PrimitiveTopology::eTriangleStrip;

  pipelineInfo2.pInputAssemblyState = &inputAssembly2;
  // pipelineInfo2.pInputAssemblyState = &inputAssembly;

  // cull frontfaces 
  vk::PipelineRasterizationStateCreateInfo rasterizer2{};
  rasterizer2.lineWidth = 1.0f;
  rasterizer2.cullMode = vk::CullModeFlagBits::eNone;
  // rasterizer2.cullMode = vk::CullModeFlagBits::eFront;
  pipelineInfo2.pRasterizationState = &rasterizer2;

  pipelineInfo2.pViewportState = &viewportState;
  pipelineInfo2.pMultisampleState = &multisampling;
  pipelineInfo2.pDepthStencilState = nullptr; // Optional


  vk::PipelineColorBlendStateCreateInfo colorBlending2{};
  colorBlending2.attachmentCount = 1;
  colorBlending2.pAttachments = &colorBlendAttachment2;
  pipelineInfo2.pColorBlendState = &colorBlending2;
  
  pipelineInfo2.pDynamicState = nullptr; // Optional
  // shade fragment only if light sphere reaches behind it
  vk::PipelineDepthStencilStateCreateInfo depthStencil2{};
  depthStencil2.depthTestEnable = VK_FALSE;
  // depthStencil2.depthTestEnable = VK_TRUE;
  depthStencil2.depthWriteEnable = VK_FALSE;
  depthStencil2.depthCompareOp = vk::CompareOp::eGreater;
  pipelineInfo2.pDepthStencilState = &depthStencil2;
  
  pipelineInfo2.renderPass = m_render_pass;
  pipelineInfo2.subpass = 1;

  pipelineInfo.flags = vk::PipelineCreateFlagBits::eAllowDerivatives;
  pipelineInfo2.flags = vk::PipelineCreateFlagBits::eAllowDerivatives;
  if (m_pipeline && m_pipeline_2) {
    pipelineInfo.flags |= vk::PipelineCreateFlagBits::eDerivative;
    pipelineInfo2.flags |= vk::PipelineCreateFlagBits::eDerivative;
    // insert previously created pipeline here to derive this one from
    pipelineInfo.basePipelineHandle = m_pipeline.get();
    pipelineInfo.basePipelineIndex = -1; // Optional
    pipelineInfo2.basePipelineHandle = m_pipeline_2.get();
    pipelineInfo2.basePipelineIndex = -1; // Optional
  }
  auto pipelines = m_device->createGraphicsPipelines(vk::PipelineCache{}, {pipelineInfo, pipelineInfo2});
  m_pipeline = pipelines[0];
  m_pipeline_2 = pipelines[1];
}

void ApplicationLod::createVertexBuffer() {
  auto bvh = model_loader::bvh("/opt/3d_models/lamure/mlod/xyzrgb_dragon_7219k.bvh");
  m_model_lod = ModelLod{m_device, bvh, "/opt/3d_models/lamure/mlod/xyzrgb_dragon_7219k.lod", NUM_NODES, 100};
  // auto bvh = model_loader::bvh("/opt/3d_models/lamure/mlod/xyzrgb_manuscript_4305k.bvh");
  // m_model_lod = ModelLod{m_device, bvh, "/opt/3d_models/lamure/mlod/xyzrgb_manuscript_4305k.lod", NUM_NODES, 16};

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
  m_device.reallocateMemoryPool("framebuffer", m_images.at("pos").memoryTypeBits(), vk::MemoryPropertyFlagBits::eDeviceLocal, m_images.at("pos").size() * 5);
  
  m_images.at("depth").bindTo(m_device.memoryPool("framebuffer"));
  m_images.at("color").bindTo(m_device.memoryPool("framebuffer"));
  m_images.at("pos").bindTo(m_device.memoryPool("framebuffer"));
  m_images.at("normal").bindTo(m_device.memoryPool("framebuffer"));
  m_images.at("color_2").bindTo(m_device.memoryPool("framebuffer"));
}

void ApplicationLod::createDepthResource() {
 auto depthFormat = findSupportedFormat(
  m_device.physical(),
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );
  auto extent = vk::Extent3D{m_swap_chain.extent().width, m_swap_chain.extent().height, 1}; 
  m_images["depth"] = m_device.createImage(extent, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment);
  m_images.at("depth").transitionToLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

  m_images["color"] = m_device.createImage(extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment);
  m_images.at("color").transitionToLayout(vk::ImageLayout::eColorAttachmentOptimal);

  m_images["pos"] = m_device.createImage(extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment);
  m_images.at("pos").transitionToLayout(vk::ImageLayout::eColorAttachmentOptimal);

  m_images["normal"] = m_device.createImage(extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment);
  m_images.at("normal").transitionToLayout(vk::ImageLayout::eColorAttachmentOptimal);

  m_images["color_2"] = m_device.createImage(extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
  m_images.at("color_2").transitionToLayout(vk::ImageLayout::eTransferSrcOptimal);

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

void ApplicationLod::createDescriptorPool() {
  // descriptor sets can be allocated for each frame resource
  m_descriptorPool = m_shaders.at("simple").createPool(m_swap_chain.numImages() - 1);
  m_descriptor_sets["textures"] = m_shaders.at("simple").allocateSet(m_descriptorPool.get(), 1);

  m_images.at("texture").writeToSet(m_descriptor_sets.at("textures"), 0, m_textureSampler.get());

  m_descriptorPool_2 = m_shaders.at("quad").createPool(1);
  m_descriptor_sets["lighting"] = m_shaders.at("quad").allocateSet(m_descriptorPool_2.get(), 1);

  m_images.at("color").writeToSet(m_descriptor_sets.at("lighting"), 0);
  m_images.at("pos").writeToSet(m_descriptor_sets.at("lighting"), 1);
  m_images.at("normal").writeToSet(m_descriptor_sets.at("lighting"), 2);
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
  ubo_cam.proj[1][1] *= -1;

  // m_device.uploadBufferData(&ubo_cam, m_buffers.at("uniform"));
}

void ApplicationLod::emptyDrawQueue() {
  // wait until draw resources are avaible before recallocation
  for (auto const& res : m_frame_resources) {
    res.waitFences();
  }
  // move all draw frames to rerecording
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_record_queue};
    while(!m_queue_draw_frames.empty()) {
      m_queue_record_frames.push(m_queue_record_frames.front());
      m_queue_draw_frames.pop();
    }
  }
}

void ApplicationLod::resize() {
  // prevent drawing during resource recreation
  { 
    std::lock_guard<std::mutex> queue_lock{m_mutex_draw_queue};
    emptyDrawQueue();
    m_device->waitIdle();
    createDepthResource();
    createRenderPass();
    createFramebuffers();

    createGraphicsPipeline();
    createDescriptorPool();
    for (auto& res : m_frame_resources) {
      updateDescriptors(res);
      updateCommandBuffers(res);
    }
  }
}

void ApplicationLod::recreatePipeline() {
  // prevent drawing during resource recreation
  { 
    std::lock_guard<std::mutex> queue_lock{m_mutex_draw_queue};
    // wait until draw resources are avaible before recallocation
    emptyDrawQueue();
    createGraphicsPipeline();
    createDescriptorPool();
    for (auto& res : m_frame_resources) {
      updateDescriptors(res);
      updateCommandBuffers(res);
    }
  }
}

///////////////////////////// misc functions ////////////////////////////////
// handle key input
void ApplicationLod::keyCallback(int key, int scancode, int action, int mods) {
}

// exe entry point
int main(int argc, char* argv[]) {
  Launcher::run<ApplicationLod>(argc, argv);
}