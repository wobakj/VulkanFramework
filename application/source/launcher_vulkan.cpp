#include "launcher_vulkan.hpp"

#include "shader_loader.hpp"
#include "model.hpp"

// c++ warpper
#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdlib>
#include <stdlib.h>
#include <cstring>
#include <functional>
#include <iostream>
#include <set>
#include <vector>
#include <fstream>

// helper functions
std::string resourcePath(int argc, char* argv[]);
void glfw_error(int error, const char* description);
void pickPhysicalDevice();

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

model model_test{};

vk::VertexInputBindingDescription model_to_bind(model const& m) {
  vk::VertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = m.vertex_bytes;
  bindingDescription.inputRate = vk::VertexInputRate::eVertex;
  return bindingDescription;  
}

std::vector<vk::VertexInputAttributeDescription> model_to_attr(model const& model_) {
  std::vector<vk::VertexInputAttributeDescription> attributeDescriptions{};

  int attrib_index = 0;
  for(model::attribute const& attribute : model::VERTEX_ATTRIBS) {
    if(model_.offsets.find(attribute) != model_.offsets.end()) {
      vk::VertexInputAttributeDescription desc{};
      desc.binding = 0;
      desc.location = attrib_index;
      desc.format = attribute.type;
      desc.offset = model_.offsets.at(attribute);
      attributeDescriptions.emplace_back(std::move(desc));
      ++attrib_index;
    }
  }
  return attributeDescriptions;  
}

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
 ,m_pipeline_layout{m_device, vkDestroyPipelineLayout}
 ,m_render_pass{m_device, vkDestroyRenderPass}
 ,m_pipeline{m_device, vkDestroyPipeline}
 ,m_sema_image_ready{m_device, vkDestroySemaphore}
 ,m_sema_render_done{m_device, vkDestroySemaphore}
 ,m_vertexBuffer{m_device, vkDestroyBuffer}
 ,m_vertexBufferMemory{m_device, vkFreeMemory}
 ,m_device{}
 // ,m_application{}
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
    resource_path += "/../resources/";
  }

  return resource_path;
}

void LauncherVulkan::initialize() {

  glfwSetErrorCallback(glfw_error);

  if (!glfwInit()) {
    std::exit(EXIT_FAILURE);
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  // create m_window, if unsuccessfull, quit
  m_window = glfwCreateWindow(m_window_width, m_window_height, "Vulkan Framework", NULL, NULL);
  if (!m_window) {
    glfwTerminate();
    std::exit(EXIT_FAILURE);
  }

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
  m_device = m_instance.createLogicalDevice(vk::SurfaceKHR{m_surface}, deviceExtensions);

  m_swap_chain = m_device.createSwapChain(vk::SurfaceKHR{m_surface}, vk::Extent2D{m_window_width, m_window_height});

  createRenderPass();
  createVertexBuffer();
  createGraphicsPipeline();
  createFramebuffers();
  createCommandBuffers();
  createSemaphores();

  // // set user pointer to access this instance statically
  glfwSetWindowUserPointer(m_window, this);
  // register resizing function
  auto resize_func = [](GLFWwindow* w, int a, int b) {
        static_cast<LauncherVulkan*>(glfwGetWindowUserPointer(w))->update_projection(w, a, b);
  };
  glfwSetFramebufferSizeCallback(m_window, resize_func);

  // // register key input function
  // auto key_func = [](GLFWwindow* w, int a, int b, int c, int d) {
  //       static_cast<LauncherVulkan*>(glfwGetWindowUserPointer(w))->key_callback(w, a, b, c, d);
  // };
  // glfwSetKeyCallback(m_window, key_func);
  // // allow free mouse movement
  // glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void LauncherVulkan::draw() {
  uint32_t imageIndex;
  auto result = m_device->acquireNextImageKHR(m_swap_chain.get(), std::numeric_limits<uint64_t>::max(), m_sema_image_ready.get(), VK_NULL_HANDLE, &imageIndex);
  if (result == vk::Result::eErrorOutOfDateKHR) {
      recreateSwapChain();
      return;
  } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
      throw std::runtime_error("failed to acquire swap chain image!");
  }

  std::vector<vk::SubmitInfo> submitInfos(1,vk::SubmitInfo{});

  vk::Semaphore waitSemaphores[]{m_sema_image_ready.get()};
  vk::PipelineStageFlags waitStages[]{vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submitInfos[0].setWaitSemaphoreCount(1);
  submitInfos[0].setPWaitSemaphores(waitSemaphores);
  submitInfos[0].setPWaitDstStageMask(waitStages);

  submitInfos[0].setCommandBufferCount(1);
  submitInfos[0].setPCommandBuffers(&m_command_buffers[imageIndex]);

  vk::Semaphore signalSemaphores[]{m_sema_render_done.get()};
  submitInfos[0].signalSemaphoreCount = 1;
  submitInfos[0].pSignalSemaphores = signalSemaphores;

  m_device.queueGraphics().submit(submitInfos, VK_NULL_HANDLE);

  vk::PresentInfoKHR presentInfo{};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  vk::SwapchainKHR swapChains[]{m_swap_chain.get()};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;
  presentInfo.pImageIndices = &imageIndex;

  presentInfo.pResults = nullptr; // Optional

  m_device.queuePresent().presentKHR(presentInfo);
}

void LauncherVulkan::createSemaphores() {
  m_sema_image_ready = m_device->createSemaphore({});
  m_sema_render_done = m_device->createSemaphore({});
}

void LauncherVulkan::createCommandBuffers() {
  if (!m_command_buffers.empty()) {
    m_device->freeCommandBuffers(m_device.pool(), m_command_buffers);
  }

  vk::CommandBufferAllocateInfo allocInfo = {};
  allocInfo.setCommandPool(m_device.pool());
  allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
  allocInfo.setCommandBufferCount((uint32_t) m_framebuffers.size());

  m_command_buffers = m_device->allocateCommandBuffers(allocInfo);

  for (size_t i = 0; i < m_command_buffers.size(); i++) {
    vk::CommandBufferBeginInfo beginInfo = {};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

    m_command_buffers[i].begin(beginInfo);

    vk::RenderPassBeginInfo renderPassInfo{};
    renderPassInfo.renderPass = m_render_pass;
    renderPassInfo.framebuffer = m_framebuffers[i];

    renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
    renderPassInfo.renderArea.extent = m_swap_chain.extent();

    vk::ClearValue clearColor = vk::ClearColorValue{std::array<float,4>{0.0f, 0.0f, 0.0f, 1.0f}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    m_command_buffers[i].beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

    m_command_buffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());

    vk::Buffer vertexBuffers[] = {m_vertexBuffer.get()};
    vk::DeviceSize offsets[] = {0};
    m_command_buffers[i].bindVertexBuffers(0, 1, vertexBuffers, offsets);

    m_command_buffers[i].draw(model_test.vertex_num, 1, 0, 0);

    m_command_buffers[i].endRenderPass();

    m_command_buffers[i].end();
  }
}

void LauncherVulkan::createFramebuffers() {
   m_framebuffers.resize(m_swap_chain.numImages(), Deleter<VkFramebuffer>{m_device, vkDestroyFramebuffer});
  for (size_t i = 0; i < m_swap_chain.numImages(); i++) {
    m_framebuffers[i] = m_device->createFramebuffer(view_to_fb(m_swap_chain.view(i), m_swap_chain.imgInfo(), m_render_pass.get()));
  }
}

void LauncherVulkan::createRenderPass() {
  vk::AttachmentDescription colorAttachment{};
  colorAttachment.format = m_swap_chain.format();
  colorAttachment.samples = vk::SampleCountFlagBits::e1;

  colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
  colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;

  colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;

  colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
  colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

  vk::AttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

  vk::SubpassDescription subPass = {};
  subPass.colorAttachmentCount = 1;
  subPass.pColorAttachments = &colorAttachmentRef;

  vk::RenderPassCreateInfo renderPassInfo{};
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subPass;
  
  vk::SubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
  dependency.srcAccessMask = vk::AccessFlagBits::eMemoryRead;
  dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
  
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  m_render_pass = m_device->createRenderPass(renderPassInfo, nullptr);
}

vk::ShaderModule LauncherVulkan::createShaderModule(const std::vector<char>& code) {
  vk::ShaderModuleCreateInfo createInfo{};
  createInfo.codeSize = code.size();
  createInfo.pCode = (uint32_t*) code.data();
  vk::ShaderModule shaderModule = m_device->createShaderModule(createInfo);

  return shaderModule;
}

void LauncherVulkan::createGraphicsPipeline() {

  Deleter<VkShaderModule> vertShaderModule{m_device, vkDestroyShaderModule};
  Deleter<VkShaderModule> fragShaderModule{m_device, vkDestroyShaderModule};

  vertShaderModule = shader_loader::module(m_resource_path + "shaders/simple_vert.spv", m_device);
  fragShaderModule = shader_loader::module(m_resource_path + "shaders/simple_frag.spv", m_device);

  vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";

  vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};


  auto bindingDescription = model_to_bind(model_test);
  auto attributeDescriptions = model_to_attr(model_test);

  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.vertexAttributeDescriptionCount = std::uint32_t(attributeDescriptions.size());
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  vk::PipelineInputAssemblyStateCreateInfo inputAssembly = {};
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
  rasterizer.frontFace = vk::FrontFace::eClockwise;

  vk::PipelineMultisampleStateCreateInfo multisampling{};

  vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment.blendEnable = VK_FALSE;

  vk::PipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  // VkDynamicState dynamicStates[] = {
  //   VK_DYNAMIC_STATE_VIEWPORT,
  //   VK_DYNAMIC_STATE_LINE_WIDTH
  // };

  // VkPipelineDynamicStateCreateInfo dynamicState = {};
  // dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  // dynamicState.dynamicStateCount = 2;
  // dynamicState.pDynamicStates = dynamicStates;

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
  m_pipeline_layout = m_device->createPipelineLayout(pipelineLayoutInfo);

  vk::GraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;

  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = nullptr; // Optional
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = nullptr; // Optional

  pipelineInfo.layout = m_pipeline_layout;

  pipelineInfo.renderPass = m_render_pass;
  pipelineInfo.subpass = 0;

  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
  pipelineInfo.basePipelineIndex = -1; // Optional

  m_pipeline = m_device->createGraphicsPipelines(vk::PipelineCache{}, pipelineInfo)[0];
}

uint32_t LauncherVulkan::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags const& properties) {
  auto memProperties = m_device.physical().getMemoryProperties();
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }
  throw std::runtime_error("failed to find suitable memory type!");
  return 0;
}

void LauncherVulkan::createVertexBuffer() {
  std::vector<float> vertex_data{
  0.0f, -0.5f, 0.5f,  1.0f, 0.0f, 0.0f,
  0.5f, 0.5f, 0.5f,   0.0f, 1.0f, 0.0f,
  -0.5f, 0.5f, 0.5f,  0.0f, 0.0f, 1.0
};

  model_test = model{vertex_data, model::POSITION | model::NORMAL};

  vk::BufferCreateInfo bufferInfo{};
  bufferInfo.size = model_test.vertex_num * model_test.vertex_bytes;
  bufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer;
  bufferInfo.sharingMode = vk::SharingMode::eExclusive;
  m_vertexBuffer = m_device->createBuffer(bufferInfo);

  auto memRequirements = m_device->getBufferMemoryRequirements(m_vertexBuffer.get());

  vk::MemoryAllocateInfo allocInfo{};
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
  m_vertexBufferMemory = m_device->allocateMemory(allocInfo);

  m_device->bindBufferMemory(m_vertexBuffer.get(), m_vertexBufferMemory.get(), 0);

  void* data = m_device->mapMemory(m_vertexBufferMemory.get(), 0, bufferInfo.size);
  std::memcpy(data, model_test.data.data(), (size_t) bufferInfo.size);
  m_device->unmapMemory(m_vertexBufferMemory.get());
}

void LauncherVulkan::createSurface() {
  if (glfwCreateWindowSurface(m_instance.get(), m_window, nullptr, m_surface.replace()) != VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }
}

void LauncherVulkan::mainLoop() {
  // do before framebuffer_resize call as it requires the projection uniform location
  // throw exception if shader compilation was unsuccessfull
  update_shader_programs(true);

  // enable depth testing
  
  // rendering loop
  while (!glfwWindowShouldClose(m_window)) {
    vkDeviceWaitIdle(m_device.get());
    // query input
    glfwPollEvents();
    // clear buffer
    // draw geometry
    draw();
    // m_application->render();
    // swap draw buffer to front
    // glfwSwapBuffers(m_window);
    // display fps
    show_fps();
  }

  quit(EXIT_SUCCESS);
}


void LauncherVulkan::recreateSwapChain() {
  m_device->waitIdle();

  m_swap_chain.recreate(vk::Extent2D{m_window_width, m_window_height});
  createRenderPass();
  createGraphicsPipeline();
  createFramebuffers();
  createCommandBuffers();
}
///////////////////////////// update functions ////////////////////////////////
// update viewport and field of view
void LauncherVulkan::update_projection(GLFWwindow* m_window, int width, int height) {
  // resize framebuffer
  // glViewport(0, 0, width, height);
  m_window_width = width;
  m_window_height = height;

  float aspect = float(width) / float(height);
  float fov_y = m_camera_fov;
  // if width is smaller, extend vertical fov 
  if (width < height) {
    fov_y = 2.0f * glm::atan(glm::tan(m_camera_fov * 0.5f) * (1.0f / aspect));
  }
  // projection is hor+ 
  glm::fmat4 camera_projection = glm::perspective(fov_y, aspect, 0.1f, 10.0f);

  if (width > 0 && height > 0) {
    recreateSwapChain();
  }
  // upload matrix to gpu
  // m_application->setProjection(camera_projection);
}

// load shader programs and update uniform locations
void LauncherVulkan::update_shader_programs(bool throwing) {
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
    update_shader_programs(false);
  }
}

// calculate fps and show in m_window title
void LauncherVulkan::show_fps() {
  ++m_frames_per_second;
  double current_time = glfwGetTime();
  if (current_time - m_last_second_time >= 1.0) {
    std::string title{"OpenGL Framework - "};
    title += std::to_string(m_frames_per_second) + " fps";

    glfwSetWindowTitle(m_window, title.c_str());
    m_frames_per_second = 0;
    m_last_second_time = current_time;
  }
}

void LauncherVulkan::quit(int status) {
  // free opengl resources
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