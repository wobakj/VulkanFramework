#include "application_lod_mpi.hpp"

#include "wrap/descriptor_pool_info.hpp"
#include "wrap/surface.hpp"
#include "texture_loader.hpp"
#include "geometry_loader.hpp"
#include "wrap/conversions.hpp"

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

cmdline::parser ApplicationLodMpi::getParser() {
  cmdline::parser cmd_parse{ApplicationThreadedTransfer::getParser()};
  cmd_parse.add<int>("cut", 'c', "cut size in MB, 0 - fourth of leaf level size", false, 0, cmdline::range(0, 1024 * 64));
  cmd_parse.add<int>("upload", 'u', "upload size in MB, 0 - 1/16 of leaf size", false, 0, cmdline::range(0, 1500));
  return cmd_parse;
}

ApplicationLodMpi::ApplicationLodMpi(std::string const& resource_path, Device& device, cmdline::parser const& cmd_parse) 
 :ApplicationThreadedTransfer<ApplicationWorker>{resource_path, device, {}, cmd_parse}
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

  createRenderResources();

  m_statistics.addAverager("gpu_copy");
  m_statistics.addAverager("gpu_draw");
  m_statistics.addAverager("uploads");

  m_statistics.addTimer("update");

  startRenderThread();
}

ApplicationLodMpi::~ApplicationLodMpi() {
  shutDown();

  double mb_per_node = double(m_model_lod.sizeNode()) / 1024.0 / 1024.0;
  std::cout << "Average upload: " << m_statistics.get("uploads") * mb_per_node << " MB"<< std::endl;
  std::cout << "Average LOD update time: " << m_statistics.get("update") << " milliseconds per node, " << m_statistics.get("update") / mb_per_node * 10.0 << " per 10 MB"<< std::endl;
  std::cout << "Average LOD transfer time: " << m_statistics.get("transfer") << " milliseconds per node, " << m_statistics.get("transfer") / mb_per_node * 10.0 << " per 10 MB"<< std::endl;
  std::cout << "Average GPU draw time: " << m_statistics.get("gpu_draw") << " milliseconds " << std::endl;
  std::cout << "Average GPU copy time: " << m_statistics.get("gpu_copy") << " milliseconds per node, " << m_statistics.get("gpu_copy") / mb_per_node * 10.0 << " per 10 MB"<< std::endl;
}

FrameResource ApplicationLodMpi::createFrameResource() {
  auto res = ApplicationThreadedTransfer::createFrameResource();
  res.addCommandBuffer("gbuffer", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.buffer_views["uniform"] = BufferView{sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer};
  res.buffer_views.at("uniform").bindTo(m_buffers.at("uniforms"));
  res.query_pools["timers"] = QueryPool{m_device, vk::QueryType::eTimestamp, 4};
  return res;
}

void ApplicationLodMpi::updateResourceDescriptors(FrameResource& res) {
  res.buffer_views.at("uniform").writeToSet(res.descriptor_sets.at("matrix"), 0, vk::DescriptorType::eUniformBuffer);
}

void ApplicationLodMpi::updateResourceCommandBuffers(FrameResource& res) {
  res.commandBuffer("gbuffer")->reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  res.commandBuffer("gbuffer")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});
  // res.query_pools.at("timers").timestamp(res.commandBuffer("gbuffer"), 1, vk::PipelineStageFlagBits::eTopOfPipe);

  res.commandBuffer("gbuffer")->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene"));

  res.commandBuffer("gbuffer")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines.at("scene").layout(), 0, {res.descriptor_sets.at("matrix"), m_descriptor_sets.at("lighting")}, {});

  res.command_buffers.at("gbuffer")->setViewport(0, vk::Viewport{0, 0, float(m_resolution.x), float(m_resolution.y), 0, 1});
  res.command_buffers.at("gbuffer")->setScissor(0, vk::Rect2D{{0, 0}, {m_resolution.x, m_resolution.y}});

  res.commandBuffer("gbuffer")->bindVertexBuffers(0, {m_model_lod.buffer()}, {0});

  res.commandBuffer("gbuffer")->drawIndirect(m_model_lod.viewDrawCommands().buffer(), m_model_lod.viewDrawCommands().offset(), uint32_t(m_model_lod.numNodes()), sizeof(vk::DrawIndirectCommand));      

  res.commandBuffer("gbuffer")->end();
}

void ApplicationLodMpi::recordTransferBuffer(FrameResource& res) {
  // read out timer values from previous draw
  // if (res.num_uploads > 0.0) {
  //   auto values = res.query_pools.at("timers").getTimes();
  //   m_statistics.add("gpu_copy", (values[1] - values[0]) / res.num_uploads);
  //   m_statistics.add("gpu_draw", (values[3] - values[2]));
  // }

  m_statistics.start("update");
  // upload node data
  m_model_lod.update(matrixView(), matrixFrustum());
  size_t curr_uploads = m_model_lod.numUploads();
  m_statistics.add("uploads", double(curr_uploads));
  if (curr_uploads > 0) {
    m_statistics.add("update", m_statistics.stopValue("update") / double(curr_uploads));
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

void ApplicationLodMpi::recordDrawBuffer(FrameResource& res) {

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
  vk::BufferMemoryBarrier barrier_buffer{};
  barrier_buffer.buffer = res.buffer_views.at("uniform").buffer();
  barrier_buffer.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
  barrier_buffer.dstAccessMask = vk::AccessFlagBits::eUniformRead;
  barrier_buffer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier_buffer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  res.commandBuffer("draw")->pipelineBarrier(
    vk::PipelineStageFlagBits::eTransfer,
    vk::PipelineStageFlagBits::eVertexShader,
    vk::DependencyFlags{},
    {},
    {barrier_buffer},
    {}
  );

  res.query_pools.at("timers").timestamp(res.commandBuffer("draw"), 2, vk::PipelineStageFlagBits::eTopOfPipe);

  // make draw commands visible to drawindirect
  vk::BufferMemoryBarrier barrier_cmds{};
  barrier_cmds.buffer = m_model_lod.viewDrawCommands().buffer();
  barrier_cmds.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
  barrier_cmds.dstAccessMask = vk::AccessFlagBits::eIndirectCommandRead;
  barrier_cmds.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier_cmds.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  res.commandBuffer("draw")->pipelineBarrier(
    vk::PipelineStageFlagBits::eTransfer,
    vk::PipelineStageFlagBits::eDrawIndirect,
    vk::DependencyFlags{},
    {},
    {barrier_cmds},
    {}
  );

  // make node data visible to vertex shader
  vk::BufferMemoryBarrier barrier_nodes{};
  barrier_nodes.buffer = m_model_lod.buffer();
  barrier_nodes.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
  barrier_nodes.dstAccessMask = vk::AccessFlagBits::eVertexAttributeRead;
  barrier_nodes.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier_nodes.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  res.commandBuffer("draw")->pipelineBarrier(
    vk::PipelineStageFlagBits::eTransfer,
    vk::PipelineStageFlagBits::eVertexInput,
    vk::DependencyFlags{},
    {},
    {barrier_nodes},
    {}
  );

  // make node level data visible to fragment shader
  vk::BufferMemoryBarrier barrier_levels{};
  barrier_levels.buffer = m_model_lod.viewNodeLevels().buffer();
  barrier_levels.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
  barrier_levels.dstAccessMask = vk::AccessFlagBits::eShaderRead;
  barrier_levels.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier_levels.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  res.commandBuffer("draw")->pipelineBarrier(
    vk::PipelineStageFlagBits::eTransfer,
    vk::PipelineStageFlagBits::eFragmentShader,
    vk::DependencyFlags{},
    {},
    {barrier_levels},
    {}
  );

  res.commandBuffer("draw")->beginRenderPass(m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.commandBuffer("draw")->executeCommands({res.commandBuffer("gbuffer")});

  res.commandBuffer("draw")->endRenderPass();
  // make sure rendering to image is done before blitting
  // barrier is now performed through renderpass dependency

  res.command_buffers.at("draw").transitionLayout(*res.target_view, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
  res.command_buffers.at("draw").copyImage(m_images.at("color").view(), vk::ImageLayout::eTransferSrcOptimal, *res.target_view, vk::ImageLayout::eTransferDstOptimal);
  res.command_buffers.at("draw").transitionLayout(*res.target_view, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

  res.query_pools.at("timers").timestamp(res.commandBuffer("draw"), 3, vk::PipelineStageFlagBits::eBottomOfPipe);
  res.commandBuffer("draw")->end();
}

void ApplicationLodMpi::createFramebuffers() {
  m_framebuffer = FrameBuffer{m_device, {&m_images.at("color"), &m_images.at("depth")}, m_render_pass};
}

void ApplicationLodMpi::createRenderPasses() {
  // create renderpass with 1 subpasses
  RenderPassInfo info_pass{};
  info_pass.setAttachment(0, m_images.at("color").format(), vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
  info_pass.setAttachment(1, m_images.at("depth").format(), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  info_pass.subPass(0).setColorAttachment(0, 0);
  info_pass.subPass(0).setDepthAttachment(1);
  m_render_pass = RenderPass{m_device, info_pass};
}

void ApplicationLodMpi::createPipelines() {
  GraphicsPipelineInfo info_pipe;
  GraphicsPipelineInfo info_pipe2;

  info_pipe.setResolution(vk::Extent2D{120, 720});
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
  info_pipe.setVertexInput(m_model_lod.vertexInfo());
  info_pipe.setPass(m_render_pass, 0);
  info_pipe.addDynamic(vk::DynamicState::eViewport);
  info_pipe.addDynamic(vk::DynamicState::eScissor);

  m_pipelines.emplace("scene", GraphicsPipeline{m_device, info_pipe, m_pipeline_cache});
}

void ApplicationLodMpi::updatePipelines() {
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

void ApplicationLodMpi::createVertexBuffer(std::string const& lod_path, std::size_t cut_budget, std::size_t upload_budget) {
  m_model_lod = GeometryLod{m_transferrer, lod_path, cut_budget, upload_budget};

  vertex_data tri = geometry_loader::obj(m_resource_path + "models/sphere.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);
  m_model_light = Geometry{m_transferrer, tri};
}

void ApplicationLodMpi::createLights() {
  std::srand(5);
  for (std::size_t i = 0; i < NUM_LIGHTS; ++i) {
    light_t light;
    light.position = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)} * 10.0f - 5.0f;
    light.color = glm::fvec3{float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX), float(rand()) / float(RAND_MAX)};
    light.radius = float(rand()) / float(RAND_MAX) * 2.5f + 2.5f;
    buff_l.lights[i] = light;
  }
  m_transferrer.uploadBufferData(&buff_l, m_buffer_views.at("light"));
}

void ApplicationLodMpi::createFramebufferAttachments() {
 auto depthFormat = findSupportedFormat(
  m_device.physical(),
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );
  auto extent = vk::Extent3D{1280, 720, 1}; 
  m_images["depth"] = ImageRes{m_device, extent, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment};
  m_transferrer.transitionToLayout(m_images.at("depth"), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("depth"));

  m_images["color"] = ImageRes{m_device, extent, vk::Format::eB8G8R8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc};
  m_transferrer.transitionToLayout(m_images.at("color"), vk::ImageLayout::eTransferSrcOptimal);
  m_allocators.at("images").allocate(m_images.at("color"));
}

void ApplicationLodMpi::createTextureImage() {
  pixel_data pix_data = texture_loader::file(m_resource_path + "textures/test.tga");

  m_images["texture"] = ImageRes{m_device, pix_data.extent, pix_data.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst};
  m_allocators.at("images").allocate(m_images.at("texture"));
  
  m_transferrer.uploadImageData(pix_data.ptr(), m_images.at("texture"), vk::ImageLayout::eShaderReadOnlyOptimal);
}

void ApplicationLodMpi::createTextureSampler() {
  m_sampler = Sampler{m_device, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat};
}

void ApplicationLodMpi::updateDescriptors() {
  m_model_lod.viewNodeLevels().writeToSet(m_descriptor_sets.at("lighting"), 1, vk::DescriptorType::eStorageBuffer);
  m_images.at("texture").view().writeToSet(m_descriptor_sets.at("lighting"), 2, m_sampler.get());
  m_buffer_views.at("light").writeToSet(m_descriptor_sets.at("lighting"), 3, vk::DescriptorType::eStorageBuffer);
}

void ApplicationLodMpi::createDescriptorPools() {
  DescriptorPoolInfo info_pool{};
  info_pool.reserve(m_shaders.at("lod"), 0, uint32_t(m_frame_resources.size()));
  info_pool.reserve(m_shaders.at("lod"), 1, 1);

  m_descriptor_pool = DescriptorPool{m_device, info_pool};

  m_descriptor_sets["lighting"] = m_descriptor_pool.allocate(m_shaders.at("lod").setLayout(1));
  for(auto& res : m_frame_resources) {
    res.descriptor_sets["matrix"] = m_descriptor_pool.allocate(m_shaders.at("lod").setLayout(0));
  }
}

void ApplicationLodMpi::createUniformBuffers() {
  m_buffers["uniforms"] = Buffer{m_device, sizeof(BufferLights) * 4, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};
  m_allocators.at("buffers").allocate(m_buffers.at("uniforms"));

  m_buffer_views["light"] = BufferView{sizeof(BufferLights), vk::BufferUsageFlagBits::eStorageBuffer};
  m_buffer_views.at("light").bindTo(m_buffers.at("uniforms"));
}

///////////////////////////// update functions ////////////////////////////////
void ApplicationLodMpi::updateView() {
  ubo_cam.model = glm::mat4();
  ubo_cam.view = matrixView();
  ubo_cam.normal = glm::inverseTranspose(ubo_cam.view * ubo_cam.model);
  ubo_cam.proj = matrixFrustum();
  ubo_cam.levels = m_setting_levels ? glm::fvec4{1.0f} : glm::fvec4{0.0};
  ubo_cam.shade = m_setting_shaded ? glm::fvec4{1.0f} : glm::fvec4{0.0};
}

///////////////////////////// misc functions ////////////////////////////////
// handle key input
void ApplicationLodMpi::keyCallback(int key, int scancode, int action, int mods) {
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

#include <mpi.h>

#include "application_present.hpp"
#include "app/launcher_win.hpp"
#include "app/launcher.hpp"

// exe entry point
int main(int argc, char* argv[]) {
  // MPI::Init (argc, argv);
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
  bool threads_ok = provided >= MPI_THREAD_FUNNELED;
  assert(threads_ok);

  MPI::COMM_WORLD.Set_errhandler(MPI::ERRORS_THROW_EXCEPTIONS);
  double time_start = MPI::Wtime();
  std::cout << "Hello World, my rank is " << MPI::COMM_WORLD.Get_rank() << " of " << MPI::COMM_WORLD.Get_size() <<", response time "<< MPI::Wtime() - time_start << std::endl;
  if (MPI::COMM_WORLD.Get_rank() == 0) {
    LauncherWin::run<ApplicationPresent>(argc, argv);
  }
  else {
    Launcher::run<ApplicationLodMpi>(argc, argv);
  }

  MPI::Finalize();
}