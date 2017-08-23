#include "application_scenegraph_clustered.hpp"

#include "app/launcher_win.hpp"
#include "wrap/descriptor_pool_info.hpp"
#include "wrap/conversions.hpp"

#include "texture_loader.hpp"
#include "geometry_loader.hpp"
#include "node/node_model.hpp"
#include "node/node_ray.hpp"
#include "visit/visitor_render.hpp"
#include "visit/visitor_node.hpp"
#include "visit/visitor_transform.hpp"
#include "visit/visitor_bbox.hpp"
#include "visit/visitor_pick.hpp"
#include "loader_scene.hpp"
#include "ray.hpp"

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/string_cast.hpp>
//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>

struct UniformBufferObject {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 model;
    glm::mat4 normal;
};

struct LightGridBufferObject {
  glm::uvec2 grid_size;
  glm::uvec2 tile_size;
  glm::uvec2 resolution;
  float near;
  float far;
  glm::vec4 frustum_corners[4];
};

cmdline::parser ApplicationScenegraphClustered::getParser() {
  return ApplicationSingle::getParser();
}

ApplicationScenegraphClustered::ApplicationScenegraphClustered(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :ApplicationSingle{resource_path, device, surf, cmd_parse}
 ,m_instance{m_device, m_command_pools.at("transfer")}
 ,m_model_loader{m_instance}
 ,m_renderer{m_instance}
 ,m_graph{"graph", m_instance}
 ,m_tileSize{32, 32}
 ,m_nearFrustumCornersClipSpace{
          glm::vec4(-1.0f, +1.0f, 0.0f, 1.0f),  // bottom left
          glm::vec4(+1.0f, +1.0f, 0.0f, 1.0f),  // bottom right
          glm::vec4(+1.0f, -1.0f, 0.0f, 1.0f),  // top right
          glm::vec4(-1.0f, -1.0f, 0.0f, 1.0f)   // top left
  }    
 ,m_fly_phase_flag{true}
 ,m_selection_phase_flag{false}
 ,m_target_navi_phase_flag{false}
 ,m_manipulation_phase_flag{false}
 ,m_target_navi_start{0.0}
 ,m_target_navi_duration{3.0}
 ,m_navigator{&surf.window()}
 ,m_cam_old_pos{0.0f}
 ,m_cam_new_pos{0.0f}
{
  // check if input file was specified
  if (cmd_parse.rest().size() != 1) {
    if (cmd_parse.rest().size() < 1) {
      std::cerr << "No filename specified" << std::endl;
    }
    else {
      std::cerr << cmd_parse.usage();
    }
    exit(0);
  }
  scene_loader::json(cmd_parse.rest()[0], m_resource_path, &m_graph);

  m_shaders.emplace("scene", Shader{m_device, {m_resource_path + "shaders/graph_renderer_vert.spv", m_resource_path + "shaders/graph_renderer_frag.spv"}});
  m_shaders.emplace("tonemapping", Shader{m_device, {m_resource_path + "shaders/fullscreen_vert.spv", m_resource_path + "shaders/tone_mapping_frag.spv"}});
  m_shaders.emplace("quad", Shader{m_device, {m_resource_path + "shaders/quad_vert.spv", m_resource_path + "shaders/deferred_clustered_pbr_frag.spv"}});
  m_shaders.emplace("compute", Shader{m_device, {m_resource_path + "shaders/light_grid_comp.spv"}});

  auto cam = m_graph.createCameraNode("cam", Camera{45.0f, m_swap_chain.aspect(), 0.1f, 500.0f, &surf.window()});
  auto geo = m_graph.createGeometryNode("ray_geo", m_resource_path + "/models/sphere.obj");
  geo->scale(glm::fvec3{.01f, 0.01f, 10.0f});
  geo->translate(glm::fvec3{0.0f, 0.00f, -3.0f});
  auto ray = std::unique_ptr<Node>{new RayNode{"ray"}};
  ray->translate(glm::fvec3{0.0f, -0.4f, 0.0f});
  auto navi = std::unique_ptr<Node>{new Node{"navi_cam"}};
  ray->addChild(std::move(geo));
  navi->addChild(std::move(cam));
  navi->addChild(std::move(ray));
  m_graph.getRoot()->addChild(std::move(navi));

  createVertexBuffer();
  createUniformBuffers();
  createTextureSamplers();

  createRenderResources();
}

ApplicationScenegraphClustered::~ApplicationScenegraphClustered() {
  shutDown();
}

FrameResource ApplicationScenegraphClustered::createFrameResource() {
  auto res = ApplicationSingle::createFrameResource();
  res.command_buffers.emplace("gbuffer", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("lighting", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("tonemapping", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("compute", m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));

  return res;
}

void ApplicationScenegraphClustered::logic() {
  static double time_last = glfwGetTime();
  // calculate delta time
  double time_current = glfwGetTime();
  m_frame_time = float(time_current - time_last);
  // float time_delta = float(time_current - time_last);
  time_last = time_current;

  if (m_fly_phase_flag) {
    m_navigator.update();
    auto navi_node = m_graph.findNode("navi_cam");
    navi_node->setLocal(m_navigator.getTransform());
      
    if (m_selection_phase_flag) {
      Ray r = dynamic_cast<RayNode*>(m_graph.getRoot()->getChild("navi_cam")->getChild("ray"))->worldRay();
      PickVisitor pick_visitor{m_instance, r};
      m_graph.accept(pick_visitor);

      if (!pick_visitor.hits().empty()) {
        Hit closest_hit = pick_visitor.hits().front();
        // Node* ptr_closest = pick_visitor.hits().front().getNode();
        float dist_closest = pick_visitor.hits().front().dist();
        for (auto const& hit : pick_visitor.hits()) {
          if (hit.dist() < dist_closest) {
            closest_hit = hit;
            dist_closest = hit.dist();
            // ptr_closest = hit.getNode();
          }
        }
          m_curr_hit = closest_hit;
          if (m_curr_hit.getNode() != nullptr)std::cout<<"curr hit" <<m_curr_hit.getNode()->getName()<<std::endl;
        startTargetNavigation();
      }
    }
  }
  if (m_target_navi_phase_flag)
  {
    double elapsed_time = glfwGetTime() - m_target_navi_start;
    if (elapsed_time < m_target_navi_duration) navigateToTarget();
    else 
    {
      auto navi_node = m_graph.findNode("navi_cam");
      m_navigator.setTransform(navi_node->getLocal());
      m_target_navi_phase_flag = false;
      m_target_navi_start = 0.0;
      m_manipulation_phase_flag = true;
      m_offset = glm::inverse(navi_node->getWorld()) * m_curr_hit.getNode()->getWorld();
      // m_navigator.update(); 
    }
  }

  if (m_manipulation_phase_flag) 
  {
    auto navi = m_graph.findNode("navi_cam");
    m_navigator.setTransform(navi->getLocal());
    m_last_translation = glm::vec3(navi->getLocal()[3]);
    m_last_rotation = m_navigator.getRotation();
    m_navigator.update();
    navi->setLocal(m_navigator.getTransform());
    manipulate();
  }


  // update transforms every frame
  TransformVisitor transform_visitor{m_instance};
  m_graph.accept(transform_visitor);

  BboxVisitor box_visitor{m_instance};
  m_graph.accept(box_visitor);
}

void ApplicationScenegraphClustered::updateLightGrid() {
  auto resolution = glm::uvec2(m_swap_chain.extent().width, m_swap_chain.extent().height);
  LightGridBufferObject lightGridBuff{};
  // compute number of required screen tiles regarding the tile size and our
  // current resolution; number of depth slices is constant and needs to be
  // the same as in the compute shader
  m_lightGridSize = glm::uvec3{
      (resolution.x + m_tileSize.x - 1) / m_tileSize.x,
      (resolution.y + m_tileSize.y - 1) / m_tileSize.y, 16};
  lightGridBuff.grid_size = glm::uvec2{m_lightGridSize.x, m_lightGridSize.y};

  // compute near frustum corners in view space
  auto invProj = glm::inverse(m_camera.projectionMatrix());
  for (unsigned int i = 0; i < 4; ++i) {
    auto corner = invProj * m_nearFrustumCornersClipSpace[i];
    lightGridBuff.frustum_corners[i] = glm::vec4(glm::vec3(corner) / corner.w, 1.0f);
  }

  // some other required values
  lightGridBuff.near = m_camera.near();
  lightGridBuff.far = m_camera.far();
  lightGridBuff.tile_size = m_tileSize;
  lightGridBuff.resolution = resolution;

  m_transferrer.uploadBufferData(&lightGridBuff,
                                 m_buffer_views.at("lightgrid"));
}

void ApplicationScenegraphClustered::updateResourceCommandBuffers(FrameResource& res) {
  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  // light grid compute
  res.command_buffers.at("compute")->reset({});
  res.command_buffers.at("compute")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});
  res.command_buffers.at("compute")->bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline_compute);
  res.command_buffers.at("compute")->bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_compute.layout(), 0, {m_descriptor_sets.at("lightgrid")}, {});

  glm::vec3 workers(8.0f);
  res.command_buffers.at("compute")->dispatch(
      uint32_t(glm::ceil(float(m_lightGridSize.x) / workers.x)),
      uint32_t(glm::ceil(float(m_lightGridSize.y) / workers.y)),
      uint32_t(glm::ceil(float(m_lightGridSize.z) / workers.z)));

  res.command_buffers.at("compute")->end();

  res.command_buffers.at("gbuffer")->reset({});

  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  // first pass
  res.command_buffers.at("gbuffer").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("gbuffer").bindPipeline(m_pipelines.at("scene"));
  res.command_buffers.at("gbuffer").bindDescriptorSets(0, {m_descriptor_sets.at("camera"), m_descriptor_sets.at("transform"), m_descriptor_sets.at("material")}, {});
  res.command_buffers.at("gbuffer")->setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("gbuffer")->setScissor(0, {m_swap_chain.asRect()});

  RenderVisitor render_visitor{};
  m_graph.accept(render_visitor);
  // draw collected models
  m_renderer.draw(res.command_buffers.at("gbuffer"), render_visitor.visibleNodes());

  res.command_buffers.at("gbuffer").end();
  //deferred shading pass 
  inheritanceInfo.subpass = 1;
  res.command_buffers.at("lighting")->reset({});
  res.command_buffers.at("lighting")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  vk::ImageMemoryBarrier barrier_image{};
  barrier_image.image = m_images.at("light_vol");
  barrier_image.subresourceRange = m_images.at("light_vol").view();
  barrier_image.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
  barrier_image.dstAccessMask = vk::AccessFlagBits::eShaderRead;
  barrier_image.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier_image.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier_image.oldLayout = vk::ImageLayout::eGeneral;
  barrier_image.newLayout = vk::ImageLayout::eGeneral;

  res.commandBuffer("lighting")->pipelineBarrier(
    vk::PipelineStageFlagBits::eComputeShader,
    vk::PipelineStageFlagBits::eFragmentShader,
    vk::DependencyFlags{},
    {},
    {},
    {barrier_image}
  );

  res.command_buffers.at("lighting")->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines.at("quad"));
  res.command_buffers.at("lighting")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines.at("quad").layout(), 0, {m_descriptor_sets.at("matrix"), m_descriptor_sets.at("lighting")}, {});
  res.command_buffers.at("lighting")->setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("lighting")->setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("lighting")->draw(4, 1, 0, 0);

  res.command_buffers.at("lighting")->end();

  // tonemapping
  inheritanceInfo.subpass = 2;
  res.command_buffers.at("tonemapping")->reset({});
  res.command_buffers.at("tonemapping")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("tonemapping")->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipelines.at("tonemapping"));
  res.command_buffers.at("tonemapping")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelines.at("tonemapping").layout(), 0, {m_descriptor_sets.at("tonemapping")}, {});
  res.command_buffers.at("tonemapping")->setViewport(0, {m_swap_chain.asViewport()});
  res.command_buffers.at("tonemapping")->setScissor(0, {m_swap_chain.asRect()});

  res.command_buffers.at("tonemapping")->draw(3, 1, 0, 0);

  res.command_buffers.at("tonemapping")->end();
}

void ApplicationScenegraphClustered::recordDrawBuffer(FrameResource& res) {
  res.command_buffers.at("draw")->reset({});

  res.command_buffers.at("draw")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  m_instance.dbLight().updateCommand(res.command_buffers.at("draw"));
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("compute")});

  // copy transform data
  m_instance.dbTransform().updateCommand(res.command_buffers.at("draw"));
  m_instance.dbCamera().updateCommand(res.command_buffers.at("draw"));

  // barrier to make light data visible to vertex shader
  vk::BufferMemoryBarrier barrier{};
  barrier.buffer = m_instance.dbLight().buffer();
  barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
  barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  res.command_buffers.at("draw")->pipelineBarrier(
    vk::PipelineStageFlagBits::eTransfer,
    vk::PipelineStageFlagBits::eFragmentShader,
    vk::DependencyFlags{},
    {},
    {barrier},
    {}
  );
  res.command_buffers.at("draw")->beginRenderPass(m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("gbuffer")});
  
  res.command_buffers.at("draw")->nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
  // execute lighting buffer
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("lighting")});

  res.command_buffers.at("draw")->nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
  // execute tonemapping buffer
  res.command_buffers.at("draw")->executeCommands({res.command_buffers.at("tonemapping")});

  res.command_buffers.at("draw")->endRenderPass();
  // make sure rendering to image is done before blitting
  // barrier is now performed through renderpass dependency

  res.command_buffers.at("draw").transitionLayout(*res.target_view, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
  res.command_buffers.at("draw").copyImage(m_images.at("tonemapping_result").view(), vk::ImageLayout::eTransferSrcOptimal, *res.target_view, vk::ImageLayout::eTransferDstOptimal);
  res.command_buffers.at("draw").transitionLayout(*res.target_view, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

  res.command_buffers.at("draw")->end();
}

void ApplicationScenegraphClustered::createFramebuffers() {
  m_framebuffer = FrameBuffer{m_device, {&m_images.at("color"), &m_images.at("pos"), &m_images.at("normal"), &m_images.at("depth"), &m_images.at("color_2"), &m_images.at("tonemapping_result")}, m_render_pass};
}

void ApplicationScenegraphClustered::createRenderPasses() {
  // create renderpass with 3 subpasses
  RenderPassInfo info_pass{3};
  info_pass.setAttachment(0, m_images.at("color").format(), vk::ImageLayout::eColorAttachmentOptimal);
  info_pass.setAttachment(1, m_images.at("pos").format(), vk::ImageLayout::eColorAttachmentOptimal);
  info_pass.setAttachment(2, m_images.at("normal").format(), vk::ImageLayout::eColorAttachmentOptimal);
  info_pass.setAttachment(3, m_images.at("depth").format(), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  info_pass.setAttachment(4, m_images.at("color_2").format(), vk::ImageLayout::eColorAttachmentOptimal);
  // result will be used after the pass
  info_pass.setAttachment(5, m_images.at("tonemapping_result").format(), vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eTransferSrcOptimal);
  //first pass receives attachment 0,1,2 as color, position and normal attachment and attachment 3 as depth attachments 
  info_pass.subPass(0).setColorAttachment(0, 0);
  info_pass.subPass(0).setColorAttachment(1, 1);
  info_pass.subPass(0).setColorAttachment(2, 2);
  info_pass.subPass(0).setDepthAttachment(3);
  // second pass receives attachments 0,1,2 and inputs and writes to 4
  info_pass.subPass(1).setColorAttachment(0, 4);
  info_pass.subPass(1).setInputAttachment(0, 0);
  info_pass.subPass(1).setInputAttachment(1, 1);
  info_pass.subPass(1).setInputAttachment(2, 2);
  info_pass.subPass(1).setDepthAttachment(3);
  // third pass receives lighting result (4) and writes to tonemapping result (5)
  info_pass.subPass(2).setColorAttachment(0, 5);
  info_pass.subPass(2).setInputAttachment(0, 4);

  m_render_pass = RenderPass{m_device, info_pass};
}

void ApplicationScenegraphClustered::createPipelines() {
  GraphicsPipelineInfo info_pipe;
  GraphicsPipelineInfo info_pipe2;
  GraphicsPipelineInfo info_pipe3;

  info_pipe.setResolution(m_swap_chain.extent());
  info_pipe.setTopology(vk::PrimitiveTopology::eTriangleList);
  
  vk::PipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = vk::CullModeFlagBits::eNone;
  // rasterizer.cullMode = vk::CullModeFlagBits::eBack;
  info_pipe.setRasterizer(rasterizer);

  vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  info_pipe.setAttachmentBlending(colorBlendAttachment, 0);
  info_pipe.setAttachmentBlending(colorBlendAttachment, 1);
  info_pipe.setAttachmentBlending(colorBlendAttachment, 2);

  info_pipe.setShader(m_shaders.at("scene"));
  info_pipe.setVertexInput(m_model.vertexInfo());
  info_pipe.setPass(m_render_pass, 0);
  info_pipe.addDynamic(vk::DynamicState::eViewport);
  info_pipe.addDynamic(vk::DynamicState::eScissor);

  // uint32_t size_uint = uint32_t(m_instance.dbMaterial().mapping().at("diffuse").size());
  // info_pipe.setSpecConstant(vk::ShaderStageFlagBits::eFragment, 0, size_uint);

  vk::PipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = vk::CompareOp::eLess;
  info_pipe.setDepthStencil(depthStencil);

  info_pipe2.setResolution(m_swap_chain.extent());
  info_pipe2.setTopology(vk::PrimitiveTopology::eTriangleStrip);
  
  vk::PipelineRasterizationStateCreateInfo rasterizer2{};
  rasterizer2.lineWidth = 1.0f;
  rasterizer2.cullMode = vk::CullModeFlagBits::eFront;
  info_pipe2.setRasterizer(rasterizer2);

  info_pipe2.setAttachmentBlending(colorBlendAttachment, 0);

  info_pipe2.setShader(m_shaders.at("quad"));
  info_pipe2.setPass(m_render_pass, 1);
  info_pipe2.addDynamic(vk::DynamicState::eViewport);
  info_pipe2.addDynamic(vk::DynamicState::eScissor);

  // pipeline for tonemapping
  info_pipe3.setResolution(m_swap_chain.extent());
  info_pipe3.setTopology(vk::PrimitiveTopology::eTriangleStrip);

  info_pipe3.setRasterizer(rasterizer2);

  info_pipe3.setAttachmentBlending(colorBlendAttachment, 0);

  info_pipe3.setShader(m_shaders.at("tonemapping"));
  info_pipe3.setPass(m_render_pass, 2);
  info_pipe3.addDynamic(vk::DynamicState::eViewport);
  info_pipe3.addDynamic(vk::DynamicState::eScissor);

  ComputePipelineInfo info_pipe_comp;
  info_pipe_comp.setShader(m_shaders.at("compute"));
  m_pipeline_compute = ComputePipeline{m_device, info_pipe_comp, m_pipeline_cache};

  m_pipelines.emplace("scene", GraphicsPipeline{m_device, info_pipe, m_pipeline_cache});
  m_pipelines.emplace("quad", GraphicsPipeline{m_device, info_pipe2, m_pipeline_cache});
  // m_pipelines.emplace("lights", GraphicsPipeline{m_device, info_pipe2, m_pipeline_cache});
  m_pipelines.emplace("tonemapping", GraphicsPipeline{m_device, info_pipe3, m_pipeline_cache});
}

void ApplicationScenegraphClustered::updatePipelines() {
  auto info_pipe = m_pipelines.at("scene").info();
  info_pipe.setShader(m_shaders.at("scene"));
  m_pipelines.at("scene").recreate(info_pipe);

  auto info_pipe2 = m_pipelines.at("quad").info();
  info_pipe2.setShader(m_shaders.at("quad"));
  m_pipelines.at("quad").recreate(info_pipe2);

  auto info_pipe3 = m_pipelines.at("tonemapping").info();
  info_pipe3.setShader(m_shaders.at("tonemapping"));
  m_pipelines.at("tonemapping").recreate(info_pipe3);


  // auto info_pipe2 = m_pipelines.at("lights").info();
  // info_pipe2.setShader(m_shaders.at("lights"));
  // m_pipelines.at("lights").recreate(info_pipe2);

  auto info_pipe_comp = m_pipeline_compute.info();
  info_pipe_comp.setShader(m_shaders.at("compute"));
  m_pipeline_compute.recreate(info_pipe_comp);
}

void ApplicationScenegraphClustered::createVertexBuffer() {
  vertex_data tri = geometry_loader::obj(m_resource_path + "models/sphere.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);
  m_model = Geometry{m_transferrer, tri};
}

// void ApplicationScenegraphClustered::createLights() {
//   // one big light for ambient
//   buff_l.lights[0].position = glm::vec3(0.0f, 4.0f, 0.0f);
//   buff_l.lights[0].radius = 25.0f;
//   buff_l.lights[0].color = glm::vec3(0.996, 0.9531, 0.8945);
//   buff_l.lights[0].intensity = 50.0f;

//   std::uniform_real_distribution<float> dis(0.0f, 1.0f);
//   // std::random_device rd;
//   std::mt19937 gen(1);  // rd() as param for different seed
//   std::function<float()> rand_float = []() {
//     return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
//   };
//   // create random lights
//   std::vector<glm::vec3> colors = {
//     glm::vec3(1.0f, 0.0f, 0.0f),
//     glm::vec3(1.0f, 1.0f, 0.0f),
//     glm::vec3(1.0f, 0.0f, 1.0f),
//     glm::vec3(0.0f, 1.0f, 1.0f),
//     glm::vec3(0.0f, 0.0f, 1.0f)
//   };
//   for (std::size_t i = 0; i < NUM_LIGHTS - 1; ++i) {
//     buff_l.lights[i + 1].position =
//         glm::vec3(dis(gen) * 10.0f - 5.0f, dis(gen) * 3.0f + 0.5f,
//                   dis(gen) * 26.0f - 13.0f);
//     buff_l.lights[i + 1].radius = dis(gen) * 2.0f + 1.0f;
//     buff_l.lights[i + 1].color = colors[i % colors.size()];
//     buff_l.lights[i + 1].intensity = dis(gen) * 30.0f + 30.0f;
//   }

//   m_transferrer.uploadBufferData(&buff_l, m_buffer_views.at("light"));
// }
void ApplicationScenegraphClustered::createFramebufferAttachments() {
 auto depthFormat = findSupportedFormat(
  m_device.physical(),
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );
  auto extent = extent_3d(m_swap_chain.extent()); 
  m_images["depth"] = ImageRes{m_device, extent, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment};
  m_transferrer.transitionToLayout(m_images.at("depth"), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("depth"));

  m_images["color"] = ImageRes{m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("color"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("color"));

  m_images["pos"] = ImageRes{m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("pos"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("pos"));

  m_images["normal"] = ImageRes{m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("normal"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("normal"));

  m_images["color_2"] = ImageRes{m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  m_transferrer.transitionToLayout(m_images.at("color_2"), vk::ImageLayout::eColorAttachmentOptimal);
  m_allocators.at("images").allocate(m_images.at("color_2"));

  m_images["tonemapping_result"] = ImageRes{m_device, extent, m_swap_chain.format(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc};
  m_transferrer.transitionToLayout(m_images.at("tonemapping_result"), vk::ImageLayout::eTransferSrcOptimal);
  m_allocators.at("images").allocate(m_images.at("tonemapping_result"));

  // update light grid so its extent is computed
  updateLightGrid();
  auto lightVolExtent =
      vk::Extent3D{m_lightGridSize.x, m_lightGridSize.y, m_lightGridSize.z};

  // light volume
  m_images["light_vol"] = ImageRes{m_device, lightVolExtent, vk::Format::eR32G32B32A32Uint, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage};
  m_allocators.at("images").allocate(m_images.at("light_vol"));
  m_transferrer.transitionToLayout(m_images.at("light_vol"), vk::ImageLayout::eGeneral);
}

void ApplicationScenegraphClustered::updateDescriptors() {
  m_images.at("color").view().writeToSet(m_descriptor_sets.at("lighting"), 0, vk::DescriptorType::eInputAttachment);
  m_images.at("pos").view().writeToSet(m_descriptor_sets.at("lighting"), 1, vk::DescriptorType::eInputAttachment);
  m_images.at("normal").view().writeToSet(m_descriptor_sets.at("lighting"), 2, vk::DescriptorType::eInputAttachment);
  m_instance.dbLight().buffer().writeToSet(m_descriptor_sets.at("lighting"), 3, vk::DescriptorType::eStorageBuffer);
  // read
  m_images.at("light_vol").view().writeToSet(m_descriptor_sets.at("lighting"), 4, m_volumeSampler.get());
  m_buffer_views.at("lightgrid").writeToSet(m_descriptor_sets.at("lighting"), 5, vk::DescriptorType::eUniformBuffer);

  m_instance.dbCamera().buffer().writeToSet(m_descriptor_sets.at("camera"), 0, vk::DescriptorType::eUniformBuffer);
  m_instance.dbCamera().buffer().writeToSet(m_descriptor_sets.at("matrix"), 0, vk::DescriptorType::eUniformBuffer);
  m_instance.dbTransform().buffer().writeToSet(m_descriptor_sets.at("transform"), 0, vk::DescriptorType::eStorageBuffer);
  m_instance.dbMaterial().buffer().writeToSet(m_descriptor_sets.at("material"), 0, vk::DescriptorType::eStorageBuffer);
  m_instance.dbTexture().writeToSet(m_descriptor_sets.at("material"), 1, m_instance.dbMaterial().mapping());

  m_images.at("color_2").view().writeToSet(m_descriptor_sets.at("tonemapping"), 0, vk::DescriptorType::eInputAttachment);

  // read
  m_instance.dbCamera().buffer().writeToSet(m_descriptor_sets.at("lightgrid"), 3, vk::DescriptorType::eUniformBuffer);
  m_instance.dbLight().buffer().writeToSet(m_descriptor_sets.at("lightgrid"), 2, vk::DescriptorType::eStorageBuffer);
  m_buffer_views.at("lightgrid").writeToSet(m_descriptor_sets.at("lightgrid"), 1, vk::DescriptorType::eUniformBuffer);
  // write
  m_images.at("light_vol").view().writeToSet(m_descriptor_sets.at("lightgrid"), 0, vk::DescriptorType::eStorageImage);
}

void ApplicationScenegraphClustered::createTextureSamplers() {
  m_volumeSampler = Sampler{m_device, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat};
}

void ApplicationScenegraphClustered::createDescriptorPools() {
  DescriptorPoolInfo info_pool{};
  info_pool.reserve(m_shaders.at("scene"), 2);
  // info_pool.reserve(m_shaders.at("lights"), 1, 2);
  info_pool.reserve(m_shaders.at("compute"), 1);
  info_pool.reserve(m_shaders.at("quad"), 1, 2);
  info_pool.reserve(m_shaders.at("tonemapping"), 1);

  m_descriptor_pool = DescriptorPool{m_device, info_pool};
  m_descriptor_sets["camera"] = m_descriptor_pool.allocate(m_shaders.at("scene").setLayout(0));
  m_descriptor_sets["transform"] = m_descriptor_pool.allocate(m_shaders.at("scene").setLayout(1));
  m_descriptor_sets["material"] = m_descriptor_pool.allocate(m_shaders.at("scene").setLayout(2));

  m_descriptor_sets["lighting"] = m_descriptor_pool.allocate(m_shaders.at("quad").setLayout(1));
  // m_descriptor_sets["lighting"] = m_descriptor_pool.allocate(m_shaders.at("lights").setLayout(1));
  m_descriptor_sets["matrix"] = m_descriptor_pool.allocate(m_shaders.at("quad").setLayout(0));

  m_descriptor_sets["lightgrid"] = m_descriptor_pool.allocate(m_shaders.at("compute").setLayout(0));
  m_descriptor_sets["tonemapping"] = m_descriptor_pool.allocate(m_shaders.at("tonemapping").setLayout(0));
}

void ApplicationScenegraphClustered::createUniformBuffers() {
  m_buffers["uniforms"] = Buffer{m_device, (sizeof(LightGridBufferObject)) * 2, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};
  // m_buffer_views["light"] = BufferView{sizeof(BufferLights), vk::BufferUsageFlagBits::eStorageBuffer};
  // m_buffer_views["uniform"] = BufferView{sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer};
  m_buffer_views["lightgrid"] = BufferView{
      sizeof(LightGridBufferObject), vk::BufferUsageFlagBits::eUniformBuffer};

  m_allocators.at("buffers").allocate(m_buffers.at("uniforms"));

  m_buffer_views.at("lightgrid").bindTo(m_buffers.at("uniforms"));
  // m_buffer_views.at("light").bindTo(m_buffers.at("uniforms"));
  // m_buffer_views.at("uniform").bindTo(m_buffers.at("uniforms"));
}

void ApplicationScenegraphClustered::onResize() {
  auto cam = m_instance.dbCamera().get("cam");
  cam.setAspect(float(m_resolution.x) /  float(m_resolution.y));
  m_instance.dbCamera().set("cam", std::move(cam));
}

///////////////////////////// misc functions ////////////////////////////////
void ApplicationScenegraphClustered::mouseButtonCallback(int button, int action, int mods)
{
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    m_selection_phase_flag = true;
  else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    m_selection_phase_flag = false;
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    endTargetManipulation();
}

///////////////////////////// interaction ////////////////////////////////
void ApplicationScenegraphClustered::startTargetNavigation()
{
  auto navi = m_graph.findNode("navi_cam");
  auto ray = m_graph.findNode("ray");
  auto ray_world = static_cast<RayNode*>(ray);
  auto model_node = static_cast<ModelNode*>(m_curr_hit.getNode());

  m_target_navi_phase_flag = true;
  m_selection_phase_flag = false;
  m_manipulation_phase_flag = false;
  m_fly_phase_flag = false;

  m_target_navi_start = glfwGetTime();

  m_cam_old_pos = glm::vec3(navi->getLocal()[3]);

  
  auto curr_box = m_instance.dbModel().get(model_node->getModel()).getBox();
  curr_box.transform(model_node->getWorld());

  m_cam_new_pos = glm::vec3(ray_world->worldRay().direction().x, ray_world->worldRay().direction().y, ray_world->worldRay().direction().z) * (m_curr_hit.dist() * 0.5f);
  std::cout<<"dist" << m_curr_hit.dist()<<std::endl;
  std::cout<<"cam old pos " << m_cam_new_pos.x << " " << m_cam_new_pos.y << " " << m_cam_new_pos.z << std::endl;
}

void ApplicationScenegraphClustered::navigateToTarget()
{
  auto navi = m_graph.findNode("navi_cam");
  glm::vec3 cam_curr_disp = m_cam_new_pos * m_frame_time / float(m_target_navi_duration);
  navi->translate(cam_curr_disp);
}

void ApplicationScenegraphClustered::manipulate()
{
  auto navi = m_graph.findNode("navi_cam");
  m_curr_hit.getNode()->setLocal(glm::inverse(m_curr_hit.getNode()->getParent()->getWorld()) * navi->getWorld() * m_offset);
}

void ApplicationScenegraphClustered::endTargetManipulation()
{
  m_fly_phase_flag = true;
  m_selection_phase_flag = false;
  m_target_navi_phase_flag = false;
  m_manipulation_phase_flag = false;
}

// exe entry point
int main(int argc, char* argv[]) {
  LauncherWin::run<ApplicationScenegraphClustered>(argc, argv);
}