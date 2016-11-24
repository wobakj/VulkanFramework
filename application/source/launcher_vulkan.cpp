#include "launcher_vulkan.hpp"

#include "shader_loader.hpp"
#include "texture_loader.hpp"
#include "model_loader.hpp"
#include "model_t.hpp"

// c++ warpper
#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <cstdlib>
#include <stdlib.h>
#include <cstring>
#include <functional>
#include <iostream>
#include <set>
#include <vector>
#include <fstream>
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

LauncherVulkan::LauncherVulkan(int argc, char* argv[]) 
 :m_camera_fov{glm::radians(60.0f)}
 ,m_window_width{640u}
 ,m_window_height{480u}
 ,m_window{nullptr}
 ,m_last_second_time{0.0}
 ,m_frames_per_second{0u}
 ,m_resource_path{resourcePath(argc, argv)}
 ,m_validation_layers{{"VK_LAYER_LUNARG_standard_validation"}}
 ,m_instance{}
 ,m_surface{m_instance, vkDestroySurfaceKHR}
 ,m_pipeline{m_device, vkDestroyPipeline}
 ,m_pipeline_2{m_device, vkDestroyPipeline}
 ,m_sema_image_ready{m_device, vkDestroySemaphore}
 ,m_sema_render_done{m_device, vkDestroySemaphore}
 ,m_device{}
 ,m_descriptorPool{m_device, vkDestroyDescriptorPool}
 ,m_descriptorPool_2{m_device, vkDestroyDescriptorPool}
 ,m_textureSampler{m_device, vkDestroySampler}
 ,m_fence_draw{m_device, vkDestroyFence}
 ,m_model_dirty{false}
 ,m_camera{m_camera_fov, m_window_width, m_window_height, 0.1f, 500.0f, m_window}
 ,m_sphere{true}
{}

std::string resourcePath(int argc, char* argv[]) {
  std::string resource_path{};
  //first argument is resource path
  if (argc > 1) {
    resource_path = argv[1];
  }
  // no resource path specified, use default
  else {
    std::string exe_path{argv[0]};
    resource_path = exe_path.substr(0, exe_path.find_last_of("/\\"));
    resource_path += "/../../resources/";
  }

  return resource_path;
}

void LauncherVulkan::initialize() {

  glfwSetErrorCallback(glfw_error);

  if (!glfwInit()) {
    std::exit(EXIT_FAILURE);
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  // create m_window, if unsuccessfull, quit
  m_window = glfwCreateWindow(m_window_width, m_window_height, "Vulkan Framework", NULL, NULL);
  if (!m_window) {
    glfwTerminate();
    std::exit(EXIT_FAILURE);
  }
  m_camera = Camera{m_camera_fov, m_window_width, m_window_height, 0.1f, 50.0f, m_window};
  auto extensions =  vk::enumerateInstanceExtensionProperties(nullptr);

  std::cout << "available extensions:" << std::endl;

  for (const auto& extension : extensions) {
      std::cout << "\t" << extension.extensionName << std::endl;
  }

  // ugly workaround to use the local validation layers within this process
  std::string define{std::string{"VK_LAYER_PATH="} + VULKAN_LAYER_DIR}; 
  putenv(&define[0]);

  m_instance.create();
  createSurface();

  std::vector<const char*> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };
  m_device = m_instance.createLogicalDevice(vk::SurfaceKHR{m_surface}, deviceExtensions);

  m_swap_chain = m_device.createSwapChain(vk::SurfaceKHR{m_surface}, vk::Extent2D{m_window_width, m_window_height});


  createVertexBuffer();
  createUniformBuffers();
  createTextureImage();
  createTextureSampler();
  createDepthResource();
  createRenderPass();
  createGraphicsPipeline();
  createDescriptorPool();
  createFramebuffers();
  createCommandBuffers();
  updateCommandBuffers();
  createSemaphores();
  createLights();

  // // set user pointer to access this instance statically
  glfwSetWindowUserPointer(m_window, this);
  // register resizing function
  auto resize_func = [](GLFWwindow* w, int a, int b) {
        static_cast<LauncherVulkan*>(glfwGetWindowUserPointer(w))->update_projection(w, a, b);
  };
  glfwSetFramebufferSizeCallback(m_window, resize_func);

  // // register key input function
  auto key_func = [](GLFWwindow* w, int a, int b, int c, int d) {
        static_cast<LauncherVulkan*>(glfwGetWindowUserPointer(w))->key_callback(w, a, b, c, d);
  };
  glfwSetKeyCallback(m_window, key_func);
  // // allow free mouse movement
  // glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void LauncherVulkan::draw() {
  static bool first = true;

  // make sure no command buffer is in use
  if(!first) {
    m_device.waitFence(m_fence_draw.get());
  } 

  if(m_model_dirty.is_lock_free()) {
    if(m_model_dirty) {
      m_sphere = false;
      // std::swap(m_model, m_model_2);
      updateCommandBuffers();
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

  uint32_t imageIndex;
  auto result = m_device->acquireNextImageKHR(m_swap_chain, std::numeric_limits<uint64_t>::max(), m_sema_image_ready.get(), VK_NULL_HANDLE, &imageIndex);
  if (result == vk::Result::eErrorOutOfDateKHR) {
      recreateSwapChain();
      return;
  } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
      throw std::runtime_error("failed to acquire swap chain image!");
  }

  createPrimaryCommandBuffer(imageIndex);

  std::vector<vk::SubmitInfo> submitInfos(1,vk::SubmitInfo{});

  vk::Semaphore waitSemaphores[]{m_sema_image_ready.get()};
  vk::PipelineStageFlags waitStages[]{vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submitInfos[0].setWaitSemaphoreCount(1);
  submitInfos[0].setPWaitSemaphores(waitSemaphores);
  submitInfos[0].setPWaitDstStageMask(waitStages);

  submitInfos[0].setCommandBufferCount(1);
  submitInfos[0].setPCommandBuffers(&m_command_buffers.at("primary"));

  vk::Semaphore signalSemaphores[]{m_sema_render_done.get()};
  submitInfos[0].signalSemaphoreCount = 1;
  submitInfos[0].pSignalSemaphores = signalSemaphores;

  m_device->resetFences({m_fence_draw.get()});
  m_device.getQueue("graphics").submit(submitInfos, m_fence_draw.get());

  vk::PresentInfoKHR presentInfo{};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  vk::SwapchainKHR swapChains[]{m_swap_chain};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;
  presentInfo.pImageIndices = &imageIndex;

  m_device.getQueue("present").presentKHR(presentInfo);
  m_device.getQueue("present").waitIdle();

  first = false;
}

void LauncherVulkan::createSemaphores() {
  m_sema_image_ready = m_device->createSemaphore({});
  m_sema_render_done = m_device->createSemaphore({});
  m_fence_draw = m_device->createFence({});
  // m_device->resetFences({m_fence_draw.get()});

  m_fence_command = m_device->createFence({});
}

void LauncherVulkan::createCommandBuffers() {
  m_command_buffers.emplace("gbuffer", m_device.createCommandBuffer("graphics", vk::CommandBufferLevel::eSecondary));
  m_command_buffers.emplace("lighting", m_device.createCommandBuffer("graphics", vk::CommandBufferLevel::eSecondary));

  m_command_buffers.emplace("primary", m_device.createCommandBuffer("graphics"));
}

void LauncherVulkan::updateCommandBuffers() {
  m_device.waitFence(m_fence_draw.get());

  m_command_buffers.at("gbuffer").reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  vk::CommandBufferBeginInfo beginInfo{};
  beginInfo.flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse;
  beginInfo.pInheritanceInfo = &inheritanceInfo;
  // first pass
  m_command_buffers.at("gbuffer").begin(beginInfo);

  m_command_buffers.at("gbuffer").bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
  m_command_buffers.at("gbuffer").bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_shaders.at("simple").pipelineLayout(), 0, {m_descriptor_sets.at("matrix"), m_descriptor_sets.at("textures")}, {});
  // choose between sphere and house
  Model const* model = nullptr;
  if (m_sphere) {
    model = &m_model;
  }
  else {
    model = &m_model_2;
  }

  m_command_buffers.at("gbuffer").bindVertexBuffers(0, {model->buffer()}, {0});
  m_command_buffers.at("gbuffer").bindIndexBuffer(model->buffer(), model->indexOffset(), vk::IndexType::eUint32);

  m_command_buffers.at("gbuffer").drawIndexed(model->numIndices(), 1, 0, 0, 0);

  m_command_buffers.at("gbuffer").end();
  //deferred shading pass 
  inheritanceInfo.subpass = 1;
  m_command_buffers.at("lighting").reset({});
  m_command_buffers.at("lighting").begin(beginInfo);

  m_command_buffers.at("lighting").bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline_2.get());
  m_command_buffers.at("lighting").bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_shaders.at("quad").pipelineLayout(), 0, {m_descriptor_sets.at("matrix"), m_descriptor_sets.at("lighting")}, {});

  m_command_buffers.at("lighting").bindVertexBuffers(0, {m_model.buffer()}, {0});
  m_command_buffers.at("lighting").bindIndexBuffer(m_model.buffer(), m_model.indexOffset(), vk::IndexType::eUint32);

  m_command_buffers.at("lighting").drawIndexed(m_model.numIndices(), NUM_LIGHTS, 0, 0, 0);

  m_command_buffers.at("lighting").end();
}

void LauncherVulkan::createPrimaryCommandBuffer(int index_fb) {
  m_command_buffers.at("primary").reset({});

  vk::RenderPassBeginInfo renderPassInfo{};
  renderPassInfo.renderPass = m_render_pass;
  renderPassInfo.framebuffer = m_framebuffer;

  renderPassInfo.renderArea.extent = m_swap_chain.extent();
  // vk::ClearValue clearColor = vk::ClearColorValue{std::array<float,4>{0.0f, 0.0f, 0.0f, 1.0f}};
  std::vector<vk::ClearValue> clearValues{5, vk::ClearValue{}};
  clearValues[0].setColor(vk::ClearColorValue{std::array<float,4>{0.0f, 0.0f, 0.0f, 1.0f}});
  clearValues[1].setColor(vk::ClearColorValue{std::array<float,4>{0.0f, 0.0f, 0.0f, 1.0f}});
  clearValues[2].setColor(vk::ClearColorValue{std::array<float,4>{0.0f, 0.0f, 0.0f, 1.0f}});
  clearValues[3].setDepthStencil({1.0f, 0});
  clearValues[4].setColor(vk::ClearColorValue{std::array<float,4>{0.0f, 0.0f, 0.0f, 1.0f}});
  renderPassInfo.clearValueCount = std::uint32_t(clearValues.size());
  renderPassInfo.pClearValues = clearValues.data();

  vk::CommandBufferBeginInfo beginInfo{};
  beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
  m_command_buffers.at("primary").begin(beginInfo);

  m_command_buffers.at("primary").beginRenderPass(&renderPassInfo, vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  m_command_buffers.at("primary").executeCommands({m_command_buffers.at("gbuffer")});
  
  m_command_buffers.at("primary").nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
  // execute lighting buffer
  m_command_buffers.at("primary").executeCommands({m_command_buffers.at("lighting")});

  m_command_buffers.at("primary").endRenderPass();
  // make sure rendering to image is done before blitting
  // barrier is now performed through renderpass dependency

  vk::ImageBlit blit{};
  blit.srcSubresource = img_to_resource_layer(m_image_color_2.info());
  blit.dstSubresource = img_to_resource_layer(m_swap_chain.imgInfo());
  blit.srcOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};
  blit.dstOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};
  m_command_buffers.at("primary").blitImage(m_image_color_2, m_image_color_2.layout(), m_swap_chain.images()[index_fb], m_swap_chain.layout(), {blit}, vk::Filter::eNearest);

  m_command_buffers.at("primary").end();
}

void LauncherVulkan::createFramebuffers() {
  m_framebuffer = FrameBuffer{m_device, {m_image_color.view(), m_image_pos.view(), m_image_normal.view(), m_image_depth.view(), m_image_color_2.view()}, m_image_color.info(), m_render_pass};
}

void LauncherVulkan::createRenderPass() {
  //first pass receives attachment 0,1,2 as color, position and normal attachment and attachment 3 as depth attachments 
  sub_pass_t pass_1({0, 1, 2},{},3);
  // second pass receives attachments 0,1,2 and inputs and writes to 4
  sub_pass_t pass_2({4},{0,1,2});
  // m_render_pass = RenderPass{m_device, {m_image_color.info(), m_image_depth.info(), m_image_color_2.info()}, {pass_1, pass_2}};
  m_render_pass = RenderPass{m_device, {m_image_color.info(), m_image_pos.info(), m_image_normal.info(), m_image_depth.info(), m_image_color_2.info()}, {pass_1, pass_2}};
}

void LauncherVulkan::createGraphicsPipeline() {
  m_shaders["simple"] = Shader{m_device, {"../resources/shaders/simple_vert.spv", "../resources/shaders/simple_frag.spv"}};
  
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

  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
  pipelineInfo.basePipelineIndex = -1; // Optional

  m_pipeline = m_device->createGraphicsPipelines(vk::PipelineCache{}, pipelineInfo)[0];

  m_shaders["quad"] = Shader{m_device, {"../resources/shaders/lighting_vert.spv", "../resources/shaders/quad_frag.spv"}};
  auto pipelineInfo2 = m_shaders.at("quad").startPipelineInfo();
  
  pipelineInfo2.pVertexInputState = &vert_info;
  
  pipelineInfo2.pInputAssemblyState = &inputAssembly;

  // cull frontfaces 
  rasterizer.cullMode = vk::CullModeFlagBits::eFront;
  pipelineInfo2.pRasterizationState = &rasterizer;

  pipelineInfo2.pViewportState = &viewportState;
  pipelineInfo2.pMultisampleState = &multisampling;
  pipelineInfo2.pDepthStencilState = nullptr; // Optional

  colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
  colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eDstAlpha;
  colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
  colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  pipelineInfo2.pColorBlendState = &colorBlending;
  
  pipelineInfo2.pDynamicState = nullptr; // Optional

  depthStencil.depthTestEnable = VK_FALSE;
  depthStencil.depthWriteEnable = VK_FALSE;
  pipelineInfo2.pDepthStencilState = &depthStencil;
  
  pipelineInfo2.renderPass = m_render_pass;
  pipelineInfo2.subpass = 1;

  pipelineInfo2.basePipelineHandle = VK_NULL_HANDLE;
  pipelineInfo2.basePipelineIndex = -1; // Optional

  m_pipeline_2 = m_device->createGraphicsPipelines(vk::PipelineCache{}, pipelineInfo2)[0];
}

void LauncherVulkan::createVertexBuffer() {
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
void LauncherVulkan::loadModel() {
  try {
    model_t tri = model_loader::obj(m_resource_path + "models/house.obj", model_t::NORMAL | model_t::TEXCOORD);
    m_model_2 = Model{m_device, tri};
    m_model_dirty = true;
  }
  catch (std::exception& e) {
    assert(0);
  }
}

void LauncherVulkan::createLights() {
  std::srand(5);
  for (std::size_t i = 0; i < NUM_LIGHTS; ++i) {
    light_t light;
    light.position = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)} * 25.0f - 12.5f;
    light.color = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)};
    light.radius = float(rand()) / float(RAND_MAX) * 5.0f + 5.0f;
    buff_l.lights[i] = light;
  }
}

void LauncherVulkan::updateLights() {
  BufferLights temp = buff_l; 
  for (std::size_t i = 0; i < NUM_LIGHTS; ++i) {
    temp.lights[i].position = glm::fvec3{m_camera.viewMatrix() * glm::fvec4(temp.lights[i].position, 1.0f)};
  }

  m_device.uploadBufferData(&temp, m_buffer_light);
}

void LauncherVulkan::createSurface() {
  if (glfwCreateWindowSurface(m_instance.get(), m_window, nullptr, m_surface.replace()) != VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }
}

void LauncherVulkan::createMemoryPools() {
  // allocate pool for 5 32x4 fb attachments
  m_device.reallocateMemoryPool("framebuffer", m_image_pos.memoryTypeBits(), vk::MemoryPropertyFlagBits::eDeviceLocal, m_image_pos.size() * 5);
  
  m_image_depth.bindTo(m_device.memoryPool("framebuffer"));
  m_image_color.bindTo(m_device.memoryPool("framebuffer"));
  m_image_pos.bindTo(m_device.memoryPool("framebuffer"));
  m_image_normal.bindTo(m_device.memoryPool("framebuffer"));
  m_image_color_2.bindTo(m_device.memoryPool("framebuffer"));
}

void LauncherVulkan::createDepthResource() {
 auto depthFormat = findSupportedFormat(
  m_device.physical(),
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );

  m_image_depth = m_device.createImage(m_swap_chain.extent().width, m_swap_chain.extent().height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment);
  m_image_depth.transitionToLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

  m_image_color = m_device.createImage(m_swap_chain.extent().width, m_swap_chain.extent().height, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment);
  m_image_color.transitionToLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

  m_image_pos = m_device.createImage(m_swap_chain.extent().width, m_swap_chain.extent().height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment);
  m_image_pos.transitionToLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

  m_image_normal = m_device.createImage(m_swap_chain.extent().width, m_swap_chain.extent().height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment);
  m_image_normal.transitionToLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

  m_image_color_2 = m_device.createImage(m_swap_chain.extent().width, m_swap_chain.extent().height, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
  m_image_color_2.transitionToLayout(vk::ImageLayout::eColorAttachmentOptimal);

  createMemoryPools();
}

void LauncherVulkan::createTextureImage() {
  pixel_data pix_data = texture_loader::file(m_resource_path + "textures/test.tga");

  m_image = m_device.createImage(pix_data.width, pix_data.height, pix_data.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);
  // space for 14 8x3 1028 textures
  m_device.allocateMemoryPool("textures", m_image.memoryTypeBits(), vk::MemoryPropertyFlagBits::eDeviceLocal, m_image.size() * 16);
  m_image.bindTo(m_device.memoryPool("textures"));
  m_image.transitionToLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
  
  m_device.uploadImageData(pix_data.ptr(), m_image);
}

void LauncherVulkan::createTextureSampler() {
  vk::SamplerCreateInfo samplerInfo{};
  samplerInfo.magFilter = vk::Filter::eLinear;
  samplerInfo.minFilter = vk::Filter::eLinear;
  samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
  samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
  samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy = 16;

  m_textureSampler = m_device->createSampler(samplerInfo);
}

void LauncherVulkan::createDescriptorPool() {
  m_descriptorPool = m_shaders.at("simple").createPool(2);

  vk::DescriptorSetAllocateInfo allocInfo{};
  allocInfo.descriptorPool = m_descriptorPool;
  allocInfo.descriptorSetCount = std::uint32_t(m_shaders.at("simple").setLayouts().size());
  allocInfo.pSetLayouts = m_shaders.at("simple").setLayouts().data();

  auto sets = m_device->allocateDescriptorSets(allocInfo);
  m_descriptor_sets["matrix"] = sets[0];
  m_descriptor_sets["textures"] = sets[1];

  m_buffer_uniform.writeToSet(m_descriptor_sets.at("matrix"), 0);
  m_image.writeToSet(m_descriptor_sets.at("textures"), 0, m_textureSampler.get());

  m_descriptorPool_2 = m_shaders.at("quad").createPool(2);
  allocInfo.descriptorPool = m_descriptorPool_2;
  allocInfo.descriptorSetCount = std::uint32_t(m_shaders.at("quad").setLayouts().size());
  allocInfo.pSetLayouts = m_shaders.at("quad").setLayouts().data();

  m_descriptor_sets["lighting"] = m_device->allocateDescriptorSets(allocInfo)[1];

  m_image_color.writeToSet(m_descriptor_sets.at("lighting"), 0);
  m_image_pos.writeToSet(m_descriptor_sets.at("lighting"), 1);
  m_image_normal.writeToSet(m_descriptor_sets.at("lighting"), 2);
  m_buffer_light.writeToSet(m_descriptor_sets.at("lighting"), 3);
}

void LauncherVulkan::createUniformBuffers() {
  VkDeviceSize size = sizeof(BufferLights);
  m_buffer_light = Buffer{m_device, size, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst};

  size = sizeof(UniformBufferObject);
  m_buffer_uniform = Buffer{m_device, size, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};
  // allocate memory pool for uniforms
  m_device.allocateMemoryPool("uniforms", m_buffer_light.memoryTypeBits(), vk::MemoryPropertyFlagBits::eDeviceLocal, m_buffer_light.size() * 16);
  m_buffer_light.bindTo(m_device.memoryPool("uniforms"));
  m_buffer_uniform.bindTo(m_device.memoryPool("uniforms"));
}

void LauncherVulkan::updateUniformBuffer() {
  UniformBufferObject ubo{};
  ubo.model = glm::mat4();
  ubo.view = m_camera.viewMatrix();
  ubo.normal = glm::inverseTranspose(ubo.view * ubo.model);
  ubo.proj = m_camera.projectionMatrix();
  ubo.proj[1][1] *= -1;

  m_device.uploadBufferData(&ubo, m_buffer_uniform);

  updateLights();
}

void LauncherVulkan::mainLoop() {
  m_device->waitIdle();

  // do before framebuffer_resize call as it requires the projection uniform location
  // throw exception if shader compilation was unsuccessfull
  // update_shader_programs();

  static double time_last = 0.0;
  // enable depth testing
  // rendering loop

  while (!glfwWindowShouldClose(m_window)) {
    // claculate delta time
    double time_current = glfwGetTime();
    float time_delta = float(time_current - time_last);
    time_last = time_current;
    // update buffers
    m_camera.update(time_delta);
    if (m_camera.changed()) {
      updateUniformBuffer();
    }
    // draw geometry
    draw();
    // query input
    glfwPollEvents();
    // display fps
    show_fps();
  }

  quit(EXIT_SUCCESS);
}


void LauncherVulkan::recreateSwapChain() {
  m_device->waitIdle();

  m_swap_chain.recreate(vk::Extent2D{m_window_width, m_window_height});
  createDepthResource();
  createRenderPass();
  createGraphicsPipeline();
  createDescriptorPool();

  m_image_color.writeToSet(m_descriptor_sets.at("lighting"), 0);
  m_image_pos.writeToSet(m_descriptor_sets.at("lighting"), 1);
  m_image_normal.writeToSet(m_descriptor_sets.at("lighting"), 2);

  createFramebuffers();
  updateCommandBuffers();
}
///////////////////////////// update functions ////////////////////////////////
// update viewport and field of view
void LauncherVulkan::update_projection(GLFWwindow* m_window, int width, int height) {
  m_window_width = width;
  m_window_height = height;

  if (width > 0 && height > 0) {
    recreateSwapChain();
  }
  // update projection matrix
  m_camera.setAspect(width, height);
}

// load shader programs and update uniform locations
void LauncherVulkan::update_shader_programs() {
  // make sure pipeline is free before rebuilding
  m_device.waitFence(m_fence_draw.get());
  createGraphicsPipeline();
  updateCommandBuffers();

  // // actual functionality in lambda to allow update with and without throwing
  // auto update_lambda = [&](){
  //   // reload all shader programs
  //   for (auto& pair : m_application->getShaderPrograms()) {
  //     // throws exception when compiling was unsuccessfull
  //     GLuint new_program = shader_loader::program(pair.second.vertex_path,
  //                                                 pair.second.fragment_path);
  //     // free old shader program
  //     glDeleteProgram(pair.second.handle);
  //     // save new shader program
  //     pair.second.handle = new_program;
  //   }
  // };

  // if (throwing) {
  //   update_lambda();
  // }
  // else {
  //   try {
  //    update_lambda();
  //   }
  //   catch(std::exception&) {
  //     // dont crash, allow another try
  //   }
  // }

  // // after shader programs are recompiled, uniform locations may change
  // m_application->uploadUniforms();
  
  // // upload projection matrix to new shaders
  // int width, height;
  // glfwGetFramebufferSize(m_window, &width, &height);
  // update_projection(m_window, width, height);
}

///////////////////////////// misc functions ////////////////////////////////
// handle key input
void LauncherVulkan::key_callback(GLFWwindow* m_window, int key, int scancode, int action, int mods) {
  if ((key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(m_window, 1);
  }
  else if (key == GLFW_KEY_R && action == GLFW_PRESS) {
    update_shader_programs();
  }
  else if (key == GLFW_KEY_L && action == GLFW_PRESS) {
    if (m_sphere) {
    #ifdef THREADING
      // prevent thread creation form being triggered mutliple times
      if (!m_thread_load.joinable()) {
        m_thread_load = std::thread(&LauncherVulkan::loadModel, this);
      }
    #else
      loadModel();
    #endif
    }
  }
}

// calculate fps and show in m_window title
void LauncherVulkan::show_fps() {
  ++m_frames_per_second;
  double current_time = glfwGetTime();
  if (current_time - m_last_second_time >= 1.0) {
    std::string title{"Vulkan Framework - "};
    title += std::to_string(m_frames_per_second) + " fps";

    glfwSetWindowTitle(m_window, title.c_str());
    m_frames_per_second = 0;
    m_last_second_time = current_time;
  }
}

void LauncherVulkan::quit(int status) {
  // wait until all resources are accessible
  m_device->waitIdle();
  // free opengl resources
  for(auto const& command_buffer : m_command_buffers) {
    m_device->freeCommandBuffers(m_device.pool("graphics"), {command_buffer.second});    
  }
  // delete m_application;
  // free glfw resources
  glfwDestroyWindow(m_window);
  glfwTerminate();

  std::exit(status);
}

void glfw_error(int error, const char* description) {
  std::cerr << "GLFW Error " << error << " : "<< description << std::endl;
}

// exe entry point
int main(int argc, char* argv[]) {
  LauncherVulkan launcher{argc, argv};
  launcher.initialize();
  launcher.mainLoop();
}