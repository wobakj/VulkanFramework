#include "application_sdf.hpp"

#include "app/launcher.hpp"
#include "wrap/descriptor_pool_info.hpp"
#include "texture_loader.hpp"
#include "geometry_loader.hpp"

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <iostream>

// #define THREADING

struct SDFUniformBufferObject {
    glm::vec3 res;
    float time;
    glm::fmat4 projection;
    glm::fmat4 view;
};

struct UniformBufferObject {
    glm::fmat4 view;
    glm::fmat4 proj;
    glm::fmat4 model;
    glm::fmat4 normal;
};

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

// child classes must overwrite
const uint32_t ApplicationVulkan::imageCount = 2;

ApplicationVulkan::ApplicationVulkan(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window, cmdline::parser const& cmd_parse) 
 :ApplicationSingle{resource_path, device, chain, window, cmd_parse}
 ,m_model_dirty{false}
 ,m_sphere{true}
 ,m_database_tex{m_transferrer}
{

  m_shaders.emplace("scene", Shader{m_device, {m_resource_path + "shaders/sdf_vert.spv", m_resource_path + "shaders/sdf_frag.spv"}});  
  m_shaders.emplace("scene_model", Shader{m_device, {m_resource_path + "shaders/simple_vert.spv", m_resource_path + "shaders/simple_frag.spv"}});  
  m_shaders.emplace("lights", Shader{m_device, {m_resource_path + "shaders/lighting_vert.spv", m_resource_path + "shaders/deferred_blinn_frag.spv"}});

  createVertexBuffer();
  createUniformBuffers();
  createLights();
  createTextureImage();
  createTextureSampler();

  createRenderResources();

  // set initial camera to correct aspect
  VkExtent2D extent = m_swap_chain.extent();
  m_camera.setAspect(extent.width, extent.height);
}

ApplicationVulkan::~ApplicationVulkan() {
  shutDown();
}

FrameResource ApplicationVulkan::createFrameResource() {
  auto res = Application::createFrameResource();
  res.command_buffers.emplace("gbuffer", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("lighting", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  return res;
}

void ApplicationVulkan::updateModel() {
  emptyDrawQueue();
  m_sphere = false;
  updateResourceCommandBuffers(m_frame_resource);
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

void ApplicationVulkan::logic() {
  if(m_model_dirty.is_lock_free()) {
    if(m_model_dirty) {
      updateModel();
    }
  }
  updateView();
}

void ApplicationVulkan::updateResourceCommandBuffers(FrameResource& res) {
  res.command_buffers.at("gbuffer")->reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  // first pass
  res.command_buffers.at("gbuffer").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("gbuffer").bindPipeline(m_pipelines.at("scene"));
  res.command_buffers.at("gbuffer").bindDescriptorSets(0, {m_descriptor_sets.at("sdf_matrix")}, {});
  // glm::fvec3 test{0.0f, 1.0f, 0.0f};
  // res.command_buffers.at("gbuffer").pushConstants(vk::ShaderStageFlagBits::eFragment, 0, test);
  res.command_buffers.at("gbuffer")->setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("gbuffer")->setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("gbuffer")->draw(4, 1, 0, 0);


  res.command_buffers.at("gbuffer").bindPipeline(m_pipelines.at("scene_model"));
  res.command_buffers.at("gbuffer").bindDescriptorSets(0, {m_descriptor_sets.at("matrix"), m_descriptor_sets.at("textures")}, {});
  // glm::fvec3 test{0.0f, 1.0f, 0.0f};
  // res.command_buffers.at("gbuffer").pushConstants(vk::ShaderStageFlagBits::eFragment, 0, test);
  res.command_buffers.at("gbuffer")->setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("gbuffer")->setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("gbuffer").bindGeometry(m_model_2);

  res.command_buffers.at("gbuffer").drawGeometry(1);

  res.command_buffers.at("gbuffer").end();
  //deferred shading pass 
  inheritanceInfo.subpass = 1;
  res.command_buffers.at("lighting")->reset({});
  res.command_buffers.at("lighting").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("lighting").bindPipeline(m_pipelines.at("lights"));
  res.command_buffers.at("lighting")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines.at("lights").layout(), 0, {m_descriptor_sets.at("matrix"), m_descriptor_sets.at("lighting")}, {});
  res.command_buffers.at("lighting")->setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("lighting")->setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("lighting").bindGeometry(m_model);

  res.command_buffers.at("lighting").drawGeometry(NUM_LIGHTS);

  res.command_buffers.at("lighting").end();
}

void ApplicationVulkan::recordDrawBuffer(FrameResource& res) {
  res.command_buffers.at("draw")->reset({});

  res.command_buffers.at("draw")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  res.command_buffers.at("draw")->beginRenderPass(m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("gbuffer")});
  
  res.command_buffers.at("draw")->nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
  // execute lighting buffer
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("lighting")});

  res.command_buffers.at("draw")->endRenderPass();
  // make sure rendering to image is done before blitting
  // barrier is now performed through renderpass dependency

  vk::ImageBlit blit{};
  blit.srcSubresource = img_to_resource_layer(m_images.at("color_shaded").info());
  blit.dstSubresource = img_to_resource_layer(m_swap_chain.imgInfo());
  blit.srcOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};
  blit.dstOffsets[1] = vk::Offset3D{int(m_swap_chain.extent().width), int(m_swap_chain.extent().height), 1};

  m_swap_chain.layoutTransitionCommand(res.command_buffers.at("draw").get(), res.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
  res.command_buffers.at("draw")->blitImage(m_images.at("color_shaded"), m_images.at("color_shaded").layout(), m_swap_chain.image(res.image), m_swap_chain.layout(), {blit}, vk::Filter::eNearest); // change to color_shaded
  m_swap_chain.layoutTransitionCommand(res.command_buffers.at("draw").get(), res.image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

  res.command_buffers.at("draw")->end();
}

void ApplicationVulkan::createFramebuffers() {
  m_framebuffer = FrameBuffer{m_device, {&m_images.at("color"), &m_images.at("pos"), &m_images.at("normal"), &m_images.at("depth"), &m_images.at("color_shaded")}, m_render_pass};
}

void ApplicationVulkan::createRenderPasses() {
  //first pass receives attachment 0,1,2 as color, position and normal attachment and attachment 3 as depth attachments 
  sub_pass_t pass_1({0, 1, 2},{},3);
  // second pass receives attachments 0,1,2 and inputs and writes to 4
  sub_pass_t pass_2({4},{0,1,2}, 3);
  m_render_pass = RenderPass{m_device, {m_images.at("color").info(), m_images.at("pos").info(), m_images.at("normal").info(), m_images.at("depth").info(), m_images.at("color_shaded").info()}, {pass_1, pass_2}};
}

void ApplicationVulkan::createPipelines() {
  GraphicsPipelineInfo info_pipe;   // SDF
  GraphicsPipelineInfo info_pipe2;  // Model
  GraphicsPipelineInfo info_pipe3;  // Shady

  info_pipe.setResolution(m_swap_chain.extent());
  info_pipe.setTopology(vk::PrimitiveTopology::eTriangleStrip);
  
  vk::PipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.lineWidth = 1.0f;
  info_pipe.setRasterizer(rasterizer);

  vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  info_pipe.setAttachmentBlending(colorBlendAttachment, 0);
  info_pipe.setAttachmentBlending(colorBlendAttachment, 1);
  info_pipe.setAttachmentBlending(colorBlendAttachment, 2);

  info_pipe.setShader(m_shaders.at("scene"));
  //info_pipe.setVertexInput(m_model);
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

  info_pipe2.setResolution(m_swap_chain.extent());
  info_pipe2.setTopology(vk::PrimitiveTopology::eTriangleList);

  info_pipe2.setRasterizer(rasterizer);

  info_pipe2.setAttachmentBlending(colorBlendAttachment, 0);
  info_pipe2.setAttachmentBlending(colorBlendAttachment, 1);
  info_pipe2.setAttachmentBlending(colorBlendAttachment, 2);

  info_pipe2.setShader(m_shaders.at("scene_model"));
  info_pipe2.setVertexInput(m_model.vertexInfo());
  info_pipe2.setPass(m_render_pass, 0);
  info_pipe2.addDynamic(vk::DynamicState::eViewport);
  info_pipe2.addDynamic(vk::DynamicState::eScissor);

  // glm::fvec3 color{0.0f, 0.0f, 1.0f};
  // info_pipe2.setSpecConstant(vk::ShaderStageFlagBits::eFragment, 0, color.r);
  // info_pipe2.setSpecConstant(vk::ShaderStageFlagBits::eFragment, 1, color.g);
  // info_pipe2.setSpecConstant(vk::ShaderStageFlagBits::eFragment, 2, color.b);

  
  info_pipe2.setDepthStencil(depthStencil);

  info_pipe3.setResolution(m_swap_chain.extent());
  info_pipe3.setTopology(vk::PrimitiveTopology::eTriangleList);
  
  vk::PipelineRasterizationStateCreateInfo rasterizer3{};
  rasterizer3.lineWidth = 1.0f;
  rasterizer3.cullMode = vk::CullModeFlagBits::eFront;
  info_pipe3.setRasterizer(rasterizer3);

  vk::PipelineColorBlendAttachmentState colorBlendAttachment3{};
  colorBlendAttachment3.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment3.blendEnable = VK_TRUE;
  colorBlendAttachment3.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
  colorBlendAttachment3.dstColorBlendFactor = vk::BlendFactor::eDstAlpha;
  colorBlendAttachment3.colorBlendOp = vk::BlendOp::eAdd;
  colorBlendAttachment3.srcAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment3.dstAlphaBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment3.alphaBlendOp = vk::BlendOp::eAdd;
  info_pipe3.setAttachmentBlending(colorBlendAttachment3, 0);

  info_pipe3.setVertexInput(m_model.vertexInfo());
  info_pipe3.setShader(m_shaders.at("lights"));
  info_pipe3.setPass(m_render_pass, 1);
  info_pipe3.addDynamic(vk::DynamicState::eViewport);
  info_pipe3.addDynamic(vk::DynamicState::eScissor);

  vk::PipelineDepthStencilStateCreateInfo depthStencil3{};
  depthStencil3.depthTestEnable = VK_TRUE;
  depthStencil3.depthWriteEnable = VK_FALSE;
  depthStencil3.depthCompareOp = vk::CompareOp::eGreater;
  info_pipe3.setDepthStencil(depthStencil3);

  m_pipelines.emplace("scene", GraphicsPipeline{m_device, info_pipe, m_pipeline_cache});
  m_pipelines.emplace("scene_model", GraphicsPipeline{m_device, info_pipe2, m_pipeline_cache});
  m_pipelines.emplace("lights", GraphicsPipeline{m_device, info_pipe3, m_pipeline_cache});
}

void ApplicationVulkan::updatePipelines() {
  auto info_pipe = m_pipelines.at("scene").info();
  info_pipe.setShader(m_shaders.at("scene"));
  m_pipelines.at("scene").recreate(info_pipe);
  
  auto info_pipe2 = m_pipelines.at("scene_model").info();
  info_pipe2.setShader(m_shaders.at("scene_model"));
  m_pipelines.at("scene_model").recreate(info_pipe2);

  auto info_pipe3 = m_pipelines.at("lights").info();
  info_pipe3.setShader(m_shaders.at("lights"));
  m_pipelines.at("lights").recreate(info_pipe3);
}

void ApplicationVulkan::createVertexBuffer() {
  vertex_data tri = geometry_loader::obj(m_resource_path + "models/sphere.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);
  vertex_data tri2 = geometry_loader::obj(m_resource_path + "models/Sponza/sponza.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);
  m_model_2 = Geometry{m_transferrer, tri2};
  m_model = Geometry{m_transferrer, tri};
}

void ApplicationVulkan::loadModel() {
  vertex_data tri = geometry_loader::obj(m_resource_path + "models/Sponza/sponza.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);
  m_model_2 = Geometry{m_transferrer, tri};
  m_model_dirty = true;
}

void ApplicationVulkan::createLights() {
  std::srand(5);
  for (std::size_t i = 0; i < NUM_LIGHTS; ++i) {
    light_t light;
    light.position = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)} * 25.0f - 12.5f;
    light.color = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)};
    // light.radius = float(rand()) / float(RAND_MAX) * 5.0f + 5.0f * 100.0f;
    light.radius = float(rand()) / float(RAND_MAX) * 5.0f + 5.0f;
    buff_l.lights[i] = light;
  }
  m_transferrer.uploadBufferData(&buff_l, m_buffer_views.at("light"));
}

void ApplicationVulkan::createFramebufferAttachments() {
 auto depthFormat = findSupportedFormat(
  m_device.physical(),
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );
  auto extent = vk::Extent3D{m_swap_chain.extent().width, m_swap_chain.extent().height, 1}; 
  m_images["depth"] = Image{m_device, extent, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment};
  m_transferrer.transitionToLayout(m_images.at("depth"), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("depth"));

  m_images["color"] = Image{m_device, extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment |  vk::ImageUsageFlagBits::eTransferSrc};
  m_transferrer.transitionToLayout(m_images.at("color"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("color"));

  m_images["pos"] = Image{m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("pos"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("pos"));

  m_images["normal"] = Image{m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("normal"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("normal"));

  m_images["color_shaded"] = Image{m_device, extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc};
  m_transferrer.transitionToLayout(m_images.at("color_shaded"), vk::ImageLayout::eTransferSrcOptimal);
  m_allocators.at("images").allocate(m_images.at("color_shaded"));
}

void ApplicationVulkan::createTextureImage() {
  // pixel_data pix_data = texture_loader::file(m_resource_path + "textures/test.tga");

  // m_images["texture"] = Image{m_device, pix_data.extent, pix_data.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst};
  // m_allocators.at("images").allocate(m_images.at("texture"));
 
  // m_transferrer.transitionToLayout(m_images.at("texture"), vk::ImageLayout::eShaderReadOnlyOptimal);
  // m_transferrer.uploadImageData(pix_data.ptr(), m_images.at("texture"));

  m_database_tex.store(m_resource_path + "textures/test.tga");
}

void ApplicationVulkan::createTextureSampler() {
  m_sampler = Sampler{m_device, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat};
}

void ApplicationVulkan::updateDescriptors() {
  m_images.at("color").writeToSet(m_descriptor_sets.at("lighting"), 0, vk::DescriptorType::eInputAttachment);
  m_images.at("pos").writeToSet(m_descriptor_sets.at("lighting"), 1, vk::DescriptorType::eInputAttachment);
  m_images.at("normal").writeToSet(m_descriptor_sets.at("lighting"), 2, vk::DescriptorType::eInputAttachment);
  m_buffer_views.at("light").writeToSet(m_descriptor_sets.at("lighting"), 3, vk::DescriptorType::eStorageBuffer);

  m_buffer_views.at("sdf_uniform").writeToSet(m_descriptor_sets.at("sdf_matrix"), 0, vk::DescriptorType::eUniformBuffer);
  m_buffer_views.at("uniform").writeToSet(m_descriptor_sets.at("matrix"), 0, vk::DescriptorType::eUniformBuffer);
  m_database_tex.get(m_resource_path + "textures/test.tga").writeToSet(m_descriptor_sets.at("textures"), 0, m_sampler.get());
  // m_images.at("texture").writeToSet(m_descriptor_sets.at("textures"), 0, m_sampler.get());
}

void ApplicationVulkan::createDescriptorPools() {
  DescriptorPoolInfo info_pool{};
  info_pool.reserve(m_shaders.at("scene"), 2);
  info_pool.reserve(m_shaders.at("scene_model"), 2);
  info_pool.reserve(m_shaders.at("lights"), 2);

  m_descriptor_pool = DescriptorPool{m_device, info_pool};
  m_descriptor_sets["sdf_matrix"] = m_descriptor_pool.allocate(m_shaders.at("scene").setLayout(0));
  m_descriptor_sets["matrix"] = m_descriptor_pool.allocate(m_shaders.at("scene_model").setLayout(0));
  m_descriptor_sets["textures"] = m_descriptor_pool.allocate(m_shaders.at("scene_model").setLayout(1));
  m_descriptor_sets["lighting"] = m_descriptor_pool.allocate(m_shaders.at("lights").setLayout(1));
}

void ApplicationVulkan::createUniformBuffers() {
  m_buffers["uniforms"] = Buffer{m_device, (sizeof(SDFUniformBufferObject) + sizeof(UniformBufferObject) + sizeof(BufferLights)) * 2, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};
  m_buffer_views["light"] = BufferView{sizeof(BufferLights), vk::BufferUsageFlagBits::eStorageBuffer};
  m_buffer_views["sdf_uniform"] = BufferView{sizeof(SDFUniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer};
  m_buffer_views["uniform"] = BufferView{sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer};

  m_allocators.at("buffers").allocate(m_buffers.at("uniforms"));

  m_buffer_views.at("light").bindTo(m_buffers.at("uniforms"));
  m_buffer_views.at("sdf_uniform").bindTo(m_buffers.at("uniforms"));
  m_buffer_views.at("uniform").bindTo(m_buffers.at("uniforms"));
}

///////////////////////////// update functions ////////////////////////////////
void ApplicationVulkan::updateView() {
  SDFUniformBufferObject sdf_ubo{};
  VkExtent2D extent = m_swap_chain.extent();
  sdf_ubo.res = glm::vec3(extent.width, extent.height, 1.0);
  sdf_ubo.time = glfwGetTime();
  sdf_ubo.projection = m_camera.projectionMatrix();
  sdf_ubo.view = m_camera.viewMatrix();

  UniformBufferObject ubo{};
  ubo.view = m_camera.viewMatrix();
  ubo.proj = m_camera.projectionMatrix();
  ubo.model = glm::fmat4{1.0f};
  ubo.model = glm::scale(ubo.model, glm::vec3(0.005));
  ubo.normal = glm::inverseTranspose(ubo.view * ubo.model);

  m_transferrer.uploadBufferData(&sdf_ubo, m_buffer_views.at("sdf_uniform"));
  m_transferrer.uploadBufferData(&ubo, m_buffer_views.at("uniform"));
}

///////////////////////////// misc functions ////////////////////////////////
// handle key input
void ApplicationVulkan::keyCallback(int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_L && action == GLFW_PRESS) {
    if (m_sphere) {
    #ifdef THREADING
      // prevent thread creation form being triggered mutliple times
      if (!m_thread_load.joinable()) {
        m_thread_load = std::thread(&ApplicationVulkan::loadModel, this);
      }
    #else
      loadModel();
    #endif
    }
  }
}

// exe entry point
int main(int argc, char* argv[]) {
  Launcher::run<ApplicationVulkan>(argc, argv);
}
