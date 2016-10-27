#include "launcher_vulkan.hpp"
#include <vulkan/vulkan.hpp>
// c++ warpper

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
static std::vector<char> readFile(const std::string& filename);

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

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
 ,m_physical_device{VK_NULL_HANDLE}
 ,m_surface{m_instance, vkDestroySurfaceKHR}
 ,m_pipeline_layout{m_device.get(), vkDestroyPipelineLayout}
 ,m_render_pass{m_device.get(), vkDestroyRenderPass}
 ,m_pipeline{m_device.get(), vkDestroyPipeline}
 ,m_command_pool{m_device.get(), vkDestroyCommandPool}
 ,m_sema_image_ready{m_device.get(), vkDestroySemaphore}
 ,m_sema_render_done{m_device.get(), vkDestroySemaphore}
 ,m_device{}
 // ,m_device{vkDestroyDevice}
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

  // set OGL version explicitly 
  // glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  // glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
  // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
  // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
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
  m_physical_device = m_device.physical();

  m_swap_chain.create(m_device, m_physical_device, vk::SurfaceKHR{m_surface}, VkExtent2D{m_window_width, m_window_height});

  createRenderPass();
  createGraphicsPipeline();
  createFramebuffers();
  createCommandPool();
  createCommandBuffers();
  createSemaphores();
  // use the windows context
  // glfwMakeContextCurrent(m_window);
  // // disable vsync
  // glfwSwapInterval(0);
  // // set user pointer to access this instance statically
  // glfwSetWindowUserPointer(m_window, this);
  // // register key input function
  // auto key_func = [](GLFWwindow* w, int a, int b, int c, int d) {
  //       static_cast<LauncherVulkan*>(glfwGetWindowUserPointer(w))->key_callback(w, a, b, c, d);
  // };
  // glfwSetKeyCallback(m_window, key_func);
  // // allow free mouse movement
  // glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  // // register resizing function
  // auto resize_func = [](GLFWwindow* w, int a, int b) {
  //       static_cast<LauncherVulkan*>(glfwGetWindowUserPointer(w))->update_projection(w, a, b);
  // };
  // glfwSetFramebufferSizeCallback(m_window, resize_func);

  // initialize glindings in this context
  // glbinding::Binding::initialize();

  // activate error checking after each gl function call
  // watch_gl_errors();
}

void LauncherVulkan::draw() {
  vk::Semaphore a{m_sema_image_ready};
  vk::Semaphore b{m_sema_render_done};
  uint32_t imageIndex;
  m_device->acquireNextImageKHR(m_swap_chain.get(), std::numeric_limits<uint64_t>::max(), a, VK_NULL_HANDLE, &imageIndex);

  std::vector<vk::SubmitInfo> submitInfos(1,vk::SubmitInfo{});

  vk::Semaphore waitSemaphores[] = {m_sema_image_ready.get()};
  vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
  submitInfos[0].setWaitSemaphoreCount(1);
  submitInfos[0].setPWaitSemaphores(waitSemaphores);
  submitInfos[0].setPWaitDstStageMask(waitStages);

  submitInfos[0].setCommandBufferCount(1);
  submitInfos[0].setPCommandBuffers(&m_command_buffers[imageIndex]);

  vk::Semaphore signalSemaphores[] = {m_sema_render_done.get()};
  submitInfos[0].signalSemaphoreCount = 1;
  submitInfos[0].pSignalSemaphores = signalSemaphores;

  m_device.queueGraphics().submit(submitInfos, VK_NULL_HANDLE);

  vk::PresentInfoKHR presentInfo = {};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  vk::SwapchainKHR swapChains[] = {m_swap_chain.get()};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;
  presentInfo.pImageIndices = &imageIndex;

  presentInfo.pResults = nullptr; // Optional

  m_device.queuePresent().presentKHR(presentInfo);
}

void LauncherVulkan::createSemaphores() {
  vk::SemaphoreCreateInfo semaphoreInfo;

  vk::Semaphore a{m_sema_image_ready};
  vk::Semaphore b{m_sema_render_done};
  m_device->createSemaphore(&semaphoreInfo, nullptr, &a);
  m_device->createSemaphore(&semaphoreInfo, nullptr, &b);

  m_sema_image_ready = a; 
  m_sema_render_done = b; 
}

void LauncherVulkan::createCommandBuffers() {
  vk::CommandBufferAllocateInfo allocInfo = {};
  allocInfo.setCommandPool(m_command_pool.get());
  allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
  allocInfo.setCommandBufferCount((uint32_t) m_framebuffers.size());

  m_command_buffers = m_device->allocateCommandBuffers(allocInfo);

  for (size_t i = 0; i < m_command_buffers.size(); i++) {
    vk::CommandBufferBeginInfo beginInfo = {};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

    m_command_buffers[i].begin(beginInfo);

    vk::RenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.renderPass = m_render_pass;
    renderPassInfo.framebuffer = m_framebuffers[i];

    renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
    renderPassInfo.renderArea.extent = m_swap_chain.extend();

    vk::ClearValue clearColor = vk::ClearColorValue{std::array<float,4>{0.0f, 0.0f, 0.0f, 1.0f}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    m_command_buffers[i].beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

    m_command_buffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
    m_command_buffers[i].draw(3, 1, 0, 0);

    m_command_buffers[i].endRenderPass();

    m_command_buffers[i].end();
  }
}

void LauncherVulkan::createCommandPool() {
  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = m_device.indexGraphics();
  poolInfo.flags = 0; // Optional

  vkCreateCommandPool(m_device.get(), &poolInfo, nullptr, m_command_pool.replace());
}

void LauncherVulkan::createFramebuffers() {
   m_framebuffers.resize(m_swap_chain.views().size(), Deleter<VkFramebuffer>{m_device, vkDestroyFramebuffer});
   // std::cout << "num view " << m_swap_chain.views();
  for (size_t i = 0; i < m_swap_chain.views().size(); i++) {
    VkImageView attachments[] = {
        m_swap_chain.views()[i]
    };

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_render_pass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = m_swap_chain.extend().width;
    framebufferInfo.height = m_swap_chain.extend().height;
    framebufferInfo.layers = 1;

    vkCreateFramebuffer(m_device.get(), &framebufferInfo, nullptr, m_framebuffers[i].replace());
  }

}


void LauncherVulkan::createRenderPass() {
  vk::AttachmentDescription colorAttachment = {};
  colorAttachment.format = m_swap_chain.format();
  colorAttachment.samples = vk::SampleCountFlagBits::e1;

  colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
  colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;

  colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;

  colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
  colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

  vk::AttachmentReference colorAttachmentRef = {};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

  vk::SubpassDescription subPass = {};
  subPass.colorAttachmentCount = 1;
  subPass.pColorAttachments = &colorAttachmentRef;

  vk::RenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subPass;
  
  vk::SubpassDependency dependency = {};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
  dependency.srcAccessMask = vk::AccessFlagBits::eMemoryRead;
  dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
  
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  vk::RenderPass pass = m_device->createRenderPass(renderPassInfo, nullptr);
  m_render_pass = pass;
}

void LauncherVulkan::createShaderModule(const std::vector<char>& code, Deleter<VkShaderModule>& shaderModule) {
  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = (uint32_t*) code.data();

  vkCreateShaderModule(m_device.get(), &createInfo, nullptr, shaderModule.replace());
}

void LauncherVulkan::createGraphicsPipeline() {
  auto vertShaderCode = readFile(m_resource_path + "shaders/simple_vert.spv");
  auto fragShaderCode = readFile(m_resource_path + "shaders/simple_frag.spv");

  Deleter<VkShaderModule> vertShaderModule{m_device.get(), vkDestroyShaderModule};
  Deleter<VkShaderModule> fragShaderModule{m_device.get(), vkDestroyShaderModule};

  createShaderModule(vertShaderCode, vertShaderModule);
  createShaderModule(fragShaderCode, fragShaderModule);

  vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
  vertShaderStageInfo.module = vertShaderModule;
  vertShaderStageInfo.pName = "main";

  vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
  fragShaderStageInfo.module = fragShaderModule;
  fragShaderStageInfo.pName = "main";

  vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.vertexBindingDescriptionCount = 0;
  vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
  vertexInputInfo.vertexAttributeDescriptionCount = 0;
  vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

  vk::PipelineInputAssemblyStateCreateInfo inputAssembly = {};
  // inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  vk::Viewport viewport = {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float) m_swap_chain.extend().width;
  viewport.height = (float) m_swap_chain.extend().height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  vk::Rect2D scissor{};
  scissor.offset = vk::Offset2D{0, 0};
  scissor.extent = m_swap_chain.extend();

  vk::PipelineViewportStateCreateInfo viewportState = {};
  // viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  vk::PipelineRasterizationStateCreateInfo rasterizer = {};
  // rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = vk::PolygonMode::eFill;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = vk::CullModeFlagBits::eBack;
  rasterizer.frontFace = vk::FrontFace::eClockwise;

  rasterizer.depthBiasEnable = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.0f; // Optional
  rasterizer.depthBiasClamp = 0.0f; // Optional
  rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

  vk::PipelineMultisampleStateCreateInfo multisampling = {};
  // multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
  multisampling.minSampleShading = 1.0f; // Optional
  multisampling.pSampleMask = nullptr; /// Optional
  multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
  multisampling.alphaToOneEnable = VK_FALSE; // Optional

  vk::PipelineColorBlendAttachmentState colorBlendAttachment = {};
  colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment.blendEnable = VK_FALSE;
  colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eOne; // Optional
  colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eZero; // Optional
  colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd; // Optional
  colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne; // Optional
  colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero; // Optional
  colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd; // Optional


  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
  colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
  colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
  colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

  vk::PipelineColorBlendStateCreateInfo colorBlending = {};
  // colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = vk::LogicOp::eCopy; // Optional
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f; // Optional
  colorBlending.blendConstants[1] = 0.0f; // Optional
  colorBlending.blendConstants[2] = 0.0f; // Optional
  colorBlending.blendConstants[3] = 0.0f; // Optional

  // VkDynamicState dynamicStates[] = {
  //   VK_DYNAMIC_STATE_VIEWPORT,
  //   VK_DYNAMIC_STATE_LINE_WIDTH
  // };

  // VkPipelineDynamicStateCreateInfo dynamicState = {};
  // dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  // dynamicState.dynamicStateCount = 2;
  // dynamicState.pDynamicStates = dynamicStates;

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 0; // Optional
  pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
  pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
  pipelineLayoutInfo.pPushConstantRanges = 0; // Optional

  vkCreatePipelineLayout(m_device.get(), &pipelineLayoutInfo, nullptr,
      m_pipeline_layout.replace());

  vk::GraphicsPipelineCreateInfo pipelineInfo = {};
  // pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
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

  auto pipe = m_device->createGraphicsPipelines(vk::PipelineCache{}, pipelineInfo);
  m_pipeline = pipe[0];
}

void LauncherVulkan::createSurface() {
  if (glfwCreateWindowSurface(m_instance.get(), m_window, nullptr, m_surface.replace()) != VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }
}

void LauncherVulkan::createSwapChain() {
}

void LauncherVulkan::mainLoop() {
  // do before framebuffer_resize call as it requires the projection uniform location
  // throw exception if shader compilation was unsuccessfull
  update_shader_programs(true);

  // enable depth testing
  // glEnable(GL_DEPTH_TEST);
  // glDepthFunc(GL_LESS);
  
  // rendering loop
  while (!glfwWindowShouldClose(m_window)) {
    vkDeviceWaitIdle(m_device.get());
    // query input
    glfwPollEvents();
    // clear buffer
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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

///////////////////////////// update functions ////////////////////////////////
// update viewport and field of view
void LauncherVulkan::update_projection(GLFWwindow* m_window, int width, int height) {
  // resize framebuffer
  // glViewport(0, 0, width, height);

  float aspect = float(width) / float(height);
  float fov_y = m_camera_fov;
  // if width is smaller, extend vertical fov 
  if (width < height) {
    fov_y = 2.0f * glm::atan(glm::tan(m_camera_fov * 0.5f) * (1.0f / aspect));
  }
  // projection is hor+ 
  glm::fmat4 camera_projection = glm::perspective(fov_y, aspect, 0.1f, 10.0f);
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

static std::vector<char> readFile(const std::string& filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
      throw std::runtime_error("failed to open file!");
  }
  size_t fileSize = (size_t) file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return buffer;
}

// exe entry point
int main(int argc, char* argv[]) {
  LauncherVulkan launcher{argc, argv};
  launcher.initialize();
  launcher.mainLoop();
}