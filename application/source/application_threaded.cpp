#include "application_threaded.hpp"

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

#define THREADING
// helper functions
std::string resourcePath(int argc, char* argv[]);
void glfw_error(int error, const char* description);

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 normal;
};

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


ApplicationThreaded::ApplicationThreaded(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window) 
 :Application{resource_path, device, chain, window}
 ,m_pipeline{m_device, vkDestroyPipeline}
 ,m_pipeline_2{m_device, vkDestroyPipeline}
 ,m_descriptorPool{m_device, vkDestroyDescriptorPool}
 ,m_descriptorPool_2{m_device, vkDestroyDescriptorPool}
 ,m_textureSampler{m_device, vkDestroySampler}
 ,m_sphere{true}
 ,m_initializing{true}
 ,m_model_dirty{false}
 ,m_should_render{true}
{
  m_shaders.emplace("simple", Shader{m_device, {"../resources/shaders/simple_vert.spv", "../resources/shaders/simple_frag.spv"}});
  m_shaders.emplace("quad", Shader{m_device, {"../resources/shaders/lighting_vert.spv", "../resources/shaders/quad_frag.spv"}});

  createVertexBuffer();
  createLights();  
  createUniformBuffers();
  createTextureImage();
  createTextureSampler();
  createDepthResource();
  createRenderPass();

  createFrameResources();

  resize();
  render();

  m_initializing = false;

  if (!m_thread_render.joinable()) {
    m_thread_render = std::thread(&ApplicationThreaded::renderLoop, this);
  }
}

ApplicationThreaded::~ApplicationThreaded() {
  // shut down render thread
  m_should_render = false;
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

void ApplicationThreaded::createFrameResources() {
  // create resources for one less image than swap chain
  // only numImages - 1 images can be acquired at a time
  for (uint32_t i = 0; i < m_swap_chain.numImages() - 1; ++i) {
    m_frame_resources.emplace_back(m_device);
    createCommandBuffers(m_frame_resources.back());
    m_queue_record_frames.push(i);
  }
}

void ApplicationThreaded::acquireBackImage() {
}

void ApplicationThreaded::render() {
  // only calculate new frame if previous one was rendered
  m_mutex_record_queue.lock();
  if (m_queue_record_frames.empty()) {
    m_mutex_record_queue.unlock();
    return;
  }
  // get next frame to record
  uint32_t frame_record = m_queue_record_frames.front();
  m_queue_record_frames.pop();
  m_mutex_record_queue.unlock();
  auto& resource_record = m_frame_resources.at(frame_record);

  // acquire image after previous acquisition was successfull
  if(m_model_dirty.is_lock_free()) {
    if(m_model_dirty) {
      m_sphere = false;
      // wait until fences are done
      for (auto const& res : m_frame_resources) {
        res.waitFences();
      }
      for (auto& res : m_frame_resources) {
        updateCommandBuffers(res);
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
  }
  if (!m_initializing) {
    // wait for last acquisition until acquiring again
    m_device.waitFence(resource_record.fenceAcquire());
    m_device->resetFences(resource_record.fenceAcquire());
  }
  acquireImage(resource_record);
  // wait for drawing finish until rerecording
  m_device.waitFence(resource_record.fenceDraw());
  createPrimaryCommandBuffer(resource_record);

  // add newly recorded frame for drawing
  m_mutex_draw_queue.lock();
  m_queue_draw_frames.push(frame_record);
  m_mutex_draw_queue.unlock();
}

void ApplicationThreaded::renderLoop() {
  while (m_should_render) {
    // render only when new frame is avaible
    m_mutex_draw_queue.lock();
    if (m_queue_draw_frames.empty()) {
      m_mutex_draw_queue.unlock();
      continue;
    }
    // wait until swap chain is avaible
    m_mutex_swapchain.lock();
    // get frame to draw
    auto frame_draw = m_queue_draw_frames.front();
    m_queue_draw_frames.pop();
    m_mutex_draw_queue.unlock();
    auto& resource_draw = m_frame_resources.at(frame_draw);

    // block resource swap
    // wait until drawing is finished before issuing next draw
    m_device.waitFence(resource_draw.fenceDraw());
    submitDraw(resource_draw);
    // present image and wait for result
    present(resource_draw);
    // free swap chain
    m_mutex_swapchain.unlock();
    // make frame avaible for rerecording
    m_mutex_record_queue.lock();
    m_queue_record_frames.push(frame_draw);
    m_mutex_record_queue.unlock();
  }
}

void ApplicationThreaded::acquireImage(frame_resources_t& res) {
  auto result = m_device->acquireNextImageKHR(m_swap_chain, std::numeric_limits<uint64_t>::max(), res.semaphoreAcquire(), res.fenceAcquire(), &res.image);
  if (result == vk::Result::eErrorOutOfDateKHR) {
      // handle swapchain recreation
      // recreateSwapChain();
      throw std::runtime_error("swapchain out if date!");
  } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
      throw std::runtime_error("failed to acquire swap chain image!");
  }
}

void ApplicationThreaded::present(frame_resources_t& res) {
  vk::PresentInfoKHR presentInfo{};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &res.semaphoreDraw();

  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &m_swap_chain.get();
  presentInfo.pImageIndices = &res.image;

  m_device.getQueue("present").presentKHR(presentInfo);
  m_device.getQueue("present").waitIdle();
}

void ApplicationThreaded::submitDraw(frame_resources_t& res) {
  std::vector<vk::SubmitInfo> submitInfos(1,vk::SubmitInfo{});

  vk::Semaphore waitSemaphores[]{res.semaphoreAcquire()};
  vk::PipelineStageFlags waitStages[]{vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submitInfos[0].setWaitSemaphoreCount(1);
  submitInfos[0].setPWaitSemaphores(waitSemaphores);
  submitInfos[0].setPWaitDstStageMask(waitStages);

  submitInfos[0].setCommandBufferCount(1);
  submitInfos[0].setPCommandBuffers(&res.command_buffers.at("primary"));

  vk::Semaphore signalSemaphores[]{res.semaphoreDraw()};
  submitInfos[0].signalSemaphoreCount = 1;
  submitInfos[0].pSignalSemaphores = signalSemaphores;

  m_device->resetFences({res.fenceDraw()});
  m_device.getQueue("graphics").submit(submitInfos, res.fenceDraw());
}

void ApplicationThreaded::createCommandBuffers(frame_resources_t& resource) {
  resource.command_buffers.emplace("gbuffer", m_device.createCommandBuffer("graphics", vk::CommandBufferLevel::eSecondary));
  resource.command_buffers.emplace("lighting", m_device.createCommandBuffer("graphics", vk::CommandBufferLevel::eSecondary));
  resource.command_buffers.emplace("primary", m_device.createCommandBuffer("graphics"));
}

void ApplicationThreaded::updateCommandBuffers(frame_resources_t& resource) {
  if (!m_initializing) {
    m_device.waitFence(fenceDraw());
  }

  resource.command_buffers.at("gbuffer").reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  // first pass
  resource.command_buffers.at("gbuffer").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  resource.command_buffers.at("gbuffer").bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
  resource.command_buffers.at("gbuffer").bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_shaders.at("simple").pipelineLayout(), 0, {m_descriptor_sets.at("matrix"), m_descriptor_sets.at("textures")}, {});
  // choose between sphere and house
  Model const* model = nullptr;
  if (m_sphere) {
    model = &m_model;
  }
  else {
    model = &m_model_2;
  }

  resource.command_buffers.at("gbuffer").bindVertexBuffers(0, {model->buffer()}, {0});
  resource.command_buffers.at("gbuffer").bindIndexBuffer(model->buffer(), model->indexOffset(), vk::IndexType::eUint32);

  resource.command_buffers.at("gbuffer").drawIndexed(model->numIndices(), 1, 0, 0, 0);

  resource.command_buffers.at("gbuffer").end();
  //deferred shading pass 
  inheritanceInfo.subpass = 1;
  resource.command_buffers.at("lighting").reset({});
  resource.command_buffers.at("lighting").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  resource.command_buffers.at("lighting").bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline_2.get());
  resource.command_buffers.at("lighting").bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_shaders.at("quad").pipelineLayout(), 0, {m_descriptor_sets.at("matrix"), m_descriptor_sets.at("lighting")}, {});

  resource.command_buffers.at("lighting").bindVertexBuffers(0, {m_model.buffer()}, {0});
  resource.command_buffers.at("lighting").bindIndexBuffer(m_model.buffer(), m_model.indexOffset(), vk::IndexType::eUint32);

  resource.command_buffers.at("lighting").drawIndexed(m_model.numIndices(), NUM_LIGHTS, 0, 0, 0);

  resource.command_buffers.at("lighting").end();
}

void ApplicationThreaded::createPrimaryCommandBuffer(frame_resources_t& res) {
  res.command_buffers.at("primary").reset({});

  res.command_buffers.at("primary").begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  res.command_buffers.at("primary").beginRenderPass(m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.command_buffers.at("primary").executeCommands({res.command_buffers.at("gbuffer")});
  
  res.command_buffers.at("primary").nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
  // execute lighting buffer
  res.command_buffers.at("primary").executeCommands({res.command_buffers.at("lighting")});

  res.command_buffers.at("primary").endRenderPass();
  // make sure rendering to image is done before blitting
  // barrier is now performed through renderpass dependency

  vk::ImageBlit blit{};
  blit.srcSubresource = img_to_resource_layer(m_images.at("color_2").info());
  blit.dstSubresource = img_to_resource_layer(m_swap_chain.imgInfo());
  blit.srcOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};
  blit.dstOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};
  res.command_buffers.at("primary").blitImage(m_images.at("color_2"), m_images.at("color_2").layout(), m_swap_chain.images().at(res.image), m_swap_chain.layout(), {blit}, vk::Filter::eNearest);

  res.command_buffers.at("primary").end();
}

void ApplicationThreaded::createFramebuffers() {
  m_framebuffer = FrameBuffer{m_device, {&m_images.at("color"), &m_images.at("pos"), &m_images.at("normal"), &m_images.at("depth"), &m_images.at("color_2")}, m_render_pass};
}

void ApplicationThreaded::createRenderPass() {
  //first pass receives attachment 0,1,2 as color, position and normal attachment and attachment 3 as depth attachments 
  sub_pass_t pass_1({0, 1, 2},{},3);
  // second pass receives attachments 0,1,2 and inputs and writes to 4
  sub_pass_t pass_2({4},{0,1,2}, 3);
  m_render_pass = RenderPass{m_device, {m_images.at("color").info(), m_images.at("pos").info(), m_images.at("normal").info(), m_images.at("depth").info(), m_images.at("color_2").info()}, {pass_1, pass_2}};
}

void ApplicationThreaded::createGraphicsPipeline() {
  
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
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = vk::CullModeFlagBits::eBack;

  vk::PipelineMultisampleStateCreateInfo multisampling{};

  vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment.blendEnable = VK_FALSE;
  std::vector<vk::PipelineColorBlendAttachmentState> states{colorBlendAttachment, colorBlendAttachment, colorBlendAttachment};

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

  auto vert_info = m_model.inputInfo();
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
  
  pipelineInfo2.pVertexInputState = &vert_info;
  
  pipelineInfo2.pInputAssemblyState = &inputAssembly;

  // cull frontfaces 
  vk::PipelineRasterizationStateCreateInfo rasterizer2{};
  rasterizer2.lineWidth = 1.0f;
  rasterizer2.cullMode = vk::CullModeFlagBits::eFront;
  pipelineInfo2.pRasterizationState = &rasterizer2;

  pipelineInfo2.pViewportState = &viewportState;
  pipelineInfo2.pMultisampleState = &multisampling;
  pipelineInfo2.pDepthStencilState = nullptr; // Optional

  vk::PipelineColorBlendAttachmentState colorBlendAttachment2{};
  colorBlendAttachment2.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment2.blendEnable = VK_TRUE;
  colorBlendAttachment2.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
  colorBlendAttachment2.dstColorBlendFactor = vk::BlendFactor::eDstAlpha;
  colorBlendAttachment2.colorBlendOp = vk::BlendOp::eAdd;
  colorBlendAttachment2.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment2.dstAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment2.alphaBlendOp = vk::BlendOp::eAdd;

  vk::PipelineColorBlendStateCreateInfo colorBlending2{};
  colorBlending2.attachmentCount = 1;
  colorBlending2.pAttachments = &colorBlendAttachment2;
  pipelineInfo2.pColorBlendState = &colorBlending2;
  
  pipelineInfo2.pDynamicState = nullptr; // Optional
  // shade fragment only if light sphere reaches behind it
  vk::PipelineDepthStencilStateCreateInfo depthStencil2{};
  depthStencil2.depthTestEnable = VK_TRUE;
  depthStencil2.depthWriteEnable = VK_FALSE;
  depthStencil2.depthCompareOp = vk::CompareOp::eGreater;
  pipelineInfo2.pDepthStencilState = &depthStencil2;
  
  pipelineInfo2.renderPass = m_render_pass;
  pipelineInfo2.subpass = 1;

  pipelineInfo.flags = vk::PipelineCreateFlagBits::eAllowDerivatives;
  pipelineInfo2.flags = vk::PipelineCreateFlagBits::eAllowDerivatives;
  if (!m_initializing) {
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

void ApplicationThreaded::createVertexBuffer() {
  std::vector<float> vertex_data{
    0.0f, -0.5f, 0.5f,  1.0f, 0.0f, 0.0f,
    0.5f, 0.5f, 0.5f,   0.0f, 1.0f, 0.0f,
    -0.5f, 0.5f, 0.5f,  0.0f, 0.0f, 1.0
  };
  std::vector<std::uint32_t> indices {
    0, 1, 2
  };
  // model_t tri = model_t{vertex_data, model_t::POSITION | model_t::NORMAL, indices};

  model_t tri = model_loader::obj(m_resource_path + "models/sphere.obj", model_t::NORMAL | model_t::TEXCOORD);

  m_model = Model{m_device, tri};
}
void ApplicationThreaded::loadModel() {
  try {
    model_t tri = model_loader::obj(m_resource_path + "models/house.obj", model_t::NORMAL | model_t::TEXCOORD);
    m_model_2 = Model{m_device, tri};
    m_model_dirty = true;
  }
  catch (std::exception& e) {
    assert(0);
  }
}

void ApplicationThreaded::createLights() {
  std::srand(5);
  for (std::size_t i = 0; i < NUM_LIGHTS; ++i) {
    light_t light;
    light.position = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)} * 25.0f - 12.5f;
    light.color = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)};
    light.radius = float(rand()) / float(RAND_MAX) * 5.0f + 5.0f;
    buff_l.lights[i] = light;
  }
}

void ApplicationThreaded::updateLights() {
  BufferLights temp = buff_l; 
  for (std::size_t i = 0; i < NUM_LIGHTS; ++i) {
    temp.lights[i].position = glm::fvec3{m_camera.viewMatrix() * glm::fvec4(temp.lights[i].position, 1.0f)};
  }

  m_device.uploadBufferData(&temp, m_buffers.at("light"));
}

void ApplicationThreaded::createMemoryPools() {
  // allocate pool for 5 32x4 fb attachments
  m_device.reallocateMemoryPool("framebuffer", m_images.at("pos").memoryTypeBits(), vk::MemoryPropertyFlagBits::eDeviceLocal, m_images.at("pos").size() * 5);
  
  m_images.at("depth").bindTo(m_device.memoryPool("framebuffer"));
  m_images.at("color").bindTo(m_device.memoryPool("framebuffer"));
  m_images.at("pos").bindTo(m_device.memoryPool("framebuffer"));
  m_images.at("normal").bindTo(m_device.memoryPool("framebuffer"));
  m_images.at("color_2").bindTo(m_device.memoryPool("framebuffer"));
}

void ApplicationThreaded::createDepthResource() {
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
  m_images.at("color").transitionToLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

  m_images["pos"] = m_device.createImage(extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment);
  m_images.at("pos").transitionToLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

  m_images["normal"] = m_device.createImage(extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment);
  m_images.at("normal").transitionToLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

  m_images["color_2"] = m_device.createImage(extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
  m_images.at("color_2").transitionToLayout(vk::ImageLayout::eColorAttachmentOptimal);

  createMemoryPools();
}

void ApplicationThreaded::createTextureImage() {
  pixel_data pix_data = texture_loader::file(m_resource_path + "textures/test.tga");

  m_images["texture"] = m_device.createImage(pix_data.extent, pix_data.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
  // space for 14 8x3 1028 textures
  m_device.allocateMemoryPool("textures", m_images.at("texture").memoryTypeBits(), vk::MemoryPropertyFlagBits::eDeviceLocal, m_images.at("texture").size() * 16);
  m_images.at("texture").bindTo(m_device.memoryPool("textures"));
  m_images.at("texture").transitionToLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
  
  m_device.uploadImageData(pix_data.ptr(), m_images.at("texture"));
}

void ApplicationThreaded::createTextureSampler() {
  m_textureSampler = m_device->createSampler({{}, vk::Filter::eLinear, vk::Filter::eLinear});
}

void ApplicationThreaded::createDescriptorPool() {
  m_descriptorPool = m_shaders.at("simple").createPool(2);

  vk::DescriptorSetAllocateInfo allocInfo{};
  allocInfo.descriptorPool = m_descriptorPool;
  allocInfo.descriptorSetCount = std::uint32_t(m_shaders.at("simple").setLayouts().size());
  allocInfo.pSetLayouts = m_shaders.at("simple").setLayouts().data();

  auto sets = m_device->allocateDescriptorSets(allocInfo);
  m_descriptor_sets["matrix"] = sets[0];
  m_descriptor_sets["textures"] = sets[1];

  m_buffers.at("uniform").writeToSet(m_descriptor_sets.at("matrix"), 0);
  m_images.at("texture").writeToSet(m_descriptor_sets.at("textures"), 0, m_textureSampler.get());

  m_descriptorPool_2 = m_shaders.at("quad").createPool(2);
  allocInfo.descriptorPool = m_descriptorPool_2;
  allocInfo.descriptorSetCount = std::uint32_t(m_shaders.at("quad").setLayouts().size());
  allocInfo.pSetLayouts = m_shaders.at("quad").setLayouts().data();

  m_descriptor_sets["lighting"] = m_device->allocateDescriptorSets(allocInfo)[1];

  m_images.at("color").writeToSet(m_descriptor_sets.at("lighting"), 0);
  m_images.at("pos").writeToSet(m_descriptor_sets.at("lighting"), 1);
  m_images.at("normal").writeToSet(m_descriptor_sets.at("lighting"), 2);
  m_buffers.at("light").writeToSet(m_descriptor_sets.at("lighting"), 3);
}

void ApplicationThreaded::createUniformBuffers() {
  m_buffers["light"] = Buffer{m_device, sizeof(BufferLights), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst};
  m_buffers["uniform"] = Buffer{m_device, sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};
  // allocate memory pool for uniforms
  m_device.allocateMemoryPool("uniforms", m_buffers.at("light").memoryTypeBits(), vk::MemoryPropertyFlagBits::eDeviceLocal, m_buffers.at("light").size() * 16);
  m_buffers.at("light").bindTo(m_device.memoryPool("uniforms"));
  m_buffers.at("uniform").bindTo(m_device.memoryPool("uniforms"));
}

///////////////////////////// update functions ////////////////////////////////
void ApplicationThreaded::updateView() {
  UniformBufferObject ubo{};
  ubo.model = glm::mat4();
  ubo.view = m_camera.viewMatrix();
  ubo.normal = glm::inverseTranspose(ubo.view * ubo.model);
  ubo.proj = m_camera.projectionMatrix();
  ubo.proj[1][1] *= -1;

  m_device.uploadBufferData(&ubo, m_buffers.at("uniform"));

  updateLights();
}

void ApplicationThreaded::emptyDrawQueue() {
  // wait until draw resources are avaible before recallocation
  if (!m_initializing) {
    for (auto const& res : m_frame_resources) {
      res.waitFences();
    }
    // move all draw frames to rerecording
    m_mutex_record_queue.lock();
    while(!m_queue_draw_frames.empty()) {
      m_queue_record_frames.push(m_queue_record_frames.front());
      m_queue_draw_frames.pop();
    }
    m_mutex_record_queue.unlock();
  }
}

void ApplicationThreaded::resize() {
  // prevent drawing during resource recreation
  m_mutex_draw_queue.lock();
  emptyDrawQueue();
  createDepthResource();
  createRenderPass();
  createFramebuffers();

  createGraphicsPipeline();
  createDescriptorPool();
  for (auto& res : m_frame_resources) {
    updateCommandBuffers(res);
  }
  m_mutex_draw_queue.unlock();
}

void ApplicationThreaded::recreatePipeline() {
  // prevent drawing during resource recreation
  m_mutex_draw_queue.lock();
  // wait until draw resources are avaible before recallocation
  emptyDrawQueue();
  createGraphicsPipeline();
  createDescriptorPool();
  for (auto& res : m_frame_resources) {
    updateCommandBuffers(res);
  }
  m_mutex_draw_queue.unlock();
}

///////////////////////////// misc functions ////////////////////////////////
// handle key input
void ApplicationThreaded::keyCallback(int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_L && action == GLFW_PRESS) {
    if (m_sphere) {
    #ifdef THREADING
      // prevent thread creation form being triggered mutliple times
      if (!m_thread_load.joinable()) {
        m_thread_load = std::thread(&ApplicationThreaded::loadModel, this);
      }
    #else
      loadModel();
    #endif
    }
  }
}

// exe entry point
int main(int argc, char* argv[]) {
  Launcher::run<ApplicationThreaded>(argc, argv);
}