#include "wrap/descriptor_pool_info.hpp"
#include "wrap/render_pass.hpp"
#include "wrap/conversions.hpp"
#include "wrap/descriptor_pool.hpp"
#include "wrap/surface.hpp"

#include "texture_loader.hpp"
#include "frame_resource.hpp"
#include "frame_resource.hpp"
#include "geometry_loader.hpp"

#include "ray.hpp"
#include "loader_scene.hpp"
#include "node/node_model.hpp"
#include "node/node_navigation.hpp"
#include "node/node_ray.hpp"
#include "visit/visitor_render.hpp"
#include "visit/visitor_node.hpp"
#include "visit/visitor_transform.hpp"
#include "visit/visitor_bbox.hpp"
#include "visit/visitor_pick.hpp"

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/string_cast.hpp>
//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "cmdline.h"

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

template<typename T>
cmdline::parser ApplicationScenegraphClustered<T>::getParser() {
  return T::getParser();
}

template<typename T>
ApplicationScenegraphClustered<T>::ApplicationScenegraphClustered(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :T{resource_path, device, surf, cmd_parse}
 ,m_instance{this->m_device, this->m_command_pools.at("transfer")}
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
  scene_loader::json(cmd_parse.rest()[0], this->resourcePath(), &m_graph);

  this->m_shaders.emplace("scene", Shader{this->m_device, {this->resourcePath() + "shaders/graph_renderer_vert.spv", this->resourcePath() + "shaders/graph_renderer_frag.spv"}});
  this->m_shaders.emplace("tonemapping", Shader{this->m_device, {this->resourcePath() + "shaders/fullscreen_vert.spv", this->resourcePath() + "shaders/tone_mapping_frag.spv"}});
  this->m_shaders.emplace("quad", Shader{this->m_device, {this->resourcePath() + "shaders/quad_vert.spv", this->resourcePath() + "shaders/deferred_clustered_pbr_frag.spv"}});
  this->m_shaders.emplace("compute", Shader{this->m_device, {this->resourcePath() + "shaders/light_grid_comp.spv"}});

  auto cam = m_graph.createCameraNode("cam", Camera{45.0f, aspect(this->resolution()), 0.1f, 500.0f, &surf.window()});
  auto geo = m_graph.createGeometryNode("ray_geo", this->resourcePath() + "/models/sphere.obj");
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

  this->createRenderResources();
}

template<typename T>
ApplicationScenegraphClustered<T>::~ApplicationScenegraphClustered() {
  this->shutDown();
}

template<typename T>
FrameResource ApplicationScenegraphClustered<T>::createFrameResource() {
  auto res = T::createFrameResource();
  res.command_buffers.emplace("gbuffer", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("lighting", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("tonemapping", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("compute", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));

  return res;
}

template<typename T>
void ApplicationScenegraphClustered<T>::logic() {
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

template<typename T>
void ApplicationScenegraphClustered<T>::updateLightGrid() {
  auto resolution = glm::uvec2(extent_2d(this->resolution()).width, extent_2d(this->resolution()).height);
  LightGridBufferObject lightGridBuff{};
  // compute number of required screen tiles regarding the tile size and our
  // current resolution; number of depth slices is constant and needs to be
  // the same as in the compute shader
  m_lightGridSize = glm::uvec3{
      (resolution.x + m_tileSize.x - 1) / m_tileSize.x,
      (resolution.y + m_tileSize.y - 1) / m_tileSize.y, 16};
  lightGridBuff.grid_size = glm::uvec2{m_lightGridSize.x, m_lightGridSize.y};

  // compute near frustum corners in view space
  auto invProj = glm::inverse(this->m_camera.projectionMatrix());
  for (unsigned int i = 0; i < 4; ++i) {
    auto corner = invProj * m_nearFrustumCornersClipSpace[i];
    lightGridBuff.frustum_corners[i] = glm::vec4(glm::vec3(corner) / corner.w, 1.0f);
  }

  // some other required values
  lightGridBuff.near = this->m_camera.near();
  lightGridBuff.far = this->m_camera.far();
  lightGridBuff.tile_size = m_tileSize;
  lightGridBuff.resolution = resolution;

  this->m_transferrer.uploadBufferData(&lightGridBuff,
                                 this->m_buffer_views.at("lightgrid"));
}

template<typename T>
void ApplicationScenegraphClustered<T>::updateResourceCommandBuffers(FrameResource& res) {
  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  // light grid compute
  res.commandBuffer("compute")->reset({});
  res.commandBuffer("compute")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});
 
  // barrier to make light data visible to compute shader
  res.commandBuffer("compute").bufferBarrier(m_instance.dbLight().buffer(), 
    vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite, 
    vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderRead
  );

  res.commandBuffer("compute")->bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline_compute);
  res.commandBuffer("compute")->bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_compute.layout(), 0, {this->m_descriptor_sets.at("lightgrid")}, {});

  glm::vec3 workers(8.0f);
  res.commandBuffer("compute")->dispatch(
      uint32_t(glm::ceil(float(m_lightGridSize.x) / workers.x)),
      uint32_t(glm::ceil(float(m_lightGridSize.y) / workers.y)),
      uint32_t(glm::ceil(float(m_lightGridSize.z) / workers.z)));

  res.commandBuffer("compute")->end();

  res.commandBuffer("gbuffer")->reset({});

  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  // first pass
  res.commandBuffer("gbuffer").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.commandBuffer("gbuffer").bindPipeline(this->m_pipelines.at("scene"));
  res.commandBuffer("gbuffer").bindDescriptorSets(0, {this->m_descriptor_sets.at("camera"), this->m_descriptor_sets.at("transform"), this->m_descriptor_sets.at("material")}, {});
  res.commandBuffer("gbuffer")->setViewport(0, viewport(this->resolution()));
  res.commandBuffer("gbuffer")->setScissor(0, rect(this->resolution()));

  RenderVisitor render_visitor{};
  m_graph.accept(render_visitor);
  // draw collected models
  m_renderer.draw(res.commandBuffer("gbuffer"), render_visitor.visibleNodes());

  res.commandBuffer("gbuffer").end();
  //deferred shading pass 
  inheritanceInfo.subpass = 1;
  res.commandBuffer("lighting")->reset({});
  res.commandBuffer("lighting")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.commandBuffer("lighting")->bindPipeline(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("quad"));
  res.commandBuffer("lighting")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("quad").layout(), 0, {this->m_descriptor_sets.at("matrix"), this->m_descriptor_sets.at("lighting")}, {});
  res.commandBuffer("lighting")->setViewport(0, viewport(this->resolution()));
  res.commandBuffer("lighting")->setScissor(0, rect(this->resolution()));

  res.commandBuffer("lighting")->draw(4, 1, 0, 0);

  res.commandBuffer("lighting")->end();

  // tonemapping
  inheritanceInfo.subpass = 2;
  res.commandBuffer("tonemapping")->reset({});
  res.commandBuffer("tonemapping")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.commandBuffer("tonemapping")->bindPipeline(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("tonemapping"));
  res.commandBuffer("tonemapping")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("tonemapping").layout(), 0, {this->m_descriptor_sets.at("tonemapping")}, {});
  res.commandBuffer("tonemapping")->setViewport(0, viewport(this->resolution()));
  res.commandBuffer("tonemapping")->setScissor(0, rect(this->resolution()));

  res.commandBuffer("tonemapping")->draw(3, 1, 0, 0);

  res.commandBuffer("tonemapping")->end();
}

template<typename T>
void ApplicationScenegraphClustered<T>::recordDrawBuffer(FrameResource& res) {
  res.commandBuffer("primary")->reset({});

  res.commandBuffer("primary")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  m_instance.dbLight().updateCommand(res.commandBuffer("primary"));

  res.commandBuffer("primary")->executeCommands({res.commandBuffer("compute")});

  // copy transform data
  m_instance.dbTransform().updateCommand(res.commandBuffer("primary"));
  m_instance.dbCamera().updateCommand(res.commandBuffer("primary"));

  res.commandBuffer("primary").imageBarrier(this->m_images.at("light_vol"), vk::ImageLayout::eGeneral, 
    vk::PipelineStageFlagBits::eComputeShader, vk::AccessFlagBits::eShaderWrite, 
    vk::PipelineStageFlagBits::eFragmentShader, vk::AccessFlagBits::eShaderRead
  );

  res.commandBuffer("primary")->beginRenderPass(m_framebuffer.beginInfo(), vk::SubpassContents::eSecondaryCommandBuffers);
  // execute gbuffer creation buffer
  res.commandBuffer("primary")->executeCommands({res.commandBuffer("gbuffer")});
  
  res.commandBuffer("primary")->nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
  // execute lighting buffer
  res.commandBuffer("primary")->executeCommands({res.commandBuffer("lighting")});

  res.commandBuffer("primary")->nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
  // execute tonemapping buffer
  res.commandBuffer("primary")->executeCommands({res.commandBuffer("tonemapping")});

  res.commandBuffer("primary")->endRenderPass();

  this->presentCommands(res, this->m_images.at("tonemapping_result"), vk::ImageLayout::eTransferSrcOptimal);

  res.commandBuffer("primary")->end();
}

template<typename T>
void ApplicationScenegraphClustered<T>::createFramebuffers() {
  m_framebuffer = FrameBuffer{this->m_device, {&this->m_images.at("color"), &this->m_images.at("pos"), &this->m_images.at("normal"), &this->m_images.at("depth"), &this->m_images.at("color_2"), &this->m_images.at("tonemapping_result")}, m_render_pass};
}

template<typename T>
void ApplicationScenegraphClustered<T>::createRenderPasses() {
  // create renderpass with 3 subpasses
  RenderPassInfo info_pass{3};
  info_pass.setAttachment(0, this->m_images.at("color").format(), vk::ImageLayout::eColorAttachmentOptimal);
  info_pass.setAttachment(1, this->m_images.at("pos").format(), vk::ImageLayout::eColorAttachmentOptimal);
  info_pass.setAttachment(2, this->m_images.at("normal").format(), vk::ImageLayout::eColorAttachmentOptimal);
  info_pass.setAttachment(3, this->m_images.at("depth").format(), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  info_pass.setAttachment(4, this->m_images.at("color_2").format(), vk::ImageLayout::eColorAttachmentOptimal);
  // result will be used after the pass
  info_pass.setAttachment(5, this->m_images.at("tonemapping_result").format(), vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eTransferSrcOptimal);
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

  m_render_pass = RenderPass{this->m_device, info_pass};
}

template<typename T>
void ApplicationScenegraphClustered<T>::createPipelines() {
  GraphicsPipelineInfo info_pipe;
  GraphicsPipelineInfo info_pipe2;
  GraphicsPipelineInfo info_pipe3;

  info_pipe.setResolution(extent_2d(this->resolution()));
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

  info_pipe.setShader(this->m_shaders.at("scene"));
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

  info_pipe2.setResolution(extent_2d(this->resolution()));
  info_pipe2.setTopology(vk::PrimitiveTopology::eTriangleStrip);
  
  vk::PipelineRasterizationStateCreateInfo rasterizer2{};
  rasterizer2.lineWidth = 1.0f;
  rasterizer2.cullMode = vk::CullModeFlagBits::eFront;
  info_pipe2.setRasterizer(rasterizer2);

  info_pipe2.setAttachmentBlending(colorBlendAttachment, 0);

  info_pipe2.setShader(this->m_shaders.at("quad"));
  info_pipe2.setPass(m_render_pass, 1);
  info_pipe2.addDynamic(vk::DynamicState::eViewport);
  info_pipe2.addDynamic(vk::DynamicState::eScissor);

  // pipeline for tonemapping
  info_pipe3.setResolution(extent_2d(this->resolution()));
  info_pipe3.setTopology(vk::PrimitiveTopology::eTriangleStrip);

  info_pipe3.setRasterizer(rasterizer2);

  info_pipe3.setAttachmentBlending(colorBlendAttachment, 0);

  info_pipe3.setShader(this->m_shaders.at("tonemapping"));
  info_pipe3.setPass(m_render_pass, 2);
  info_pipe3.addDynamic(vk::DynamicState::eViewport);
  info_pipe3.addDynamic(vk::DynamicState::eScissor);

  ComputePipelineInfo info_pipe_comp;
  info_pipe_comp.setShader(this->m_shaders.at("compute"));
  m_pipeline_compute = ComputePipeline{this->m_device, info_pipe_comp, this->m_pipeline_cache};

  this->m_pipelines.emplace("scene", GraphicsPipeline{this->m_device, info_pipe, this->m_pipeline_cache});
  this->m_pipelines.emplace("quad", GraphicsPipeline{this->m_device, info_pipe2, this->m_pipeline_cache});
  // this->m_pipelines.emplace("lights", GraphicsPipeline{this->m_device, info_pipe2, this->m_pipeline_cache});
  this->m_pipelines.emplace("tonemapping", GraphicsPipeline{this->m_device, info_pipe3, this->m_pipeline_cache});
}

template<typename T>
void ApplicationScenegraphClustered<T>::updatePipelines() {
  auto info_pipe = this->m_pipelines.at("scene").info();
  info_pipe.setShader(this->m_shaders.at("scene"));
  this->m_pipelines.at("scene").recreate(info_pipe);

  auto info_pipe2 = this->m_pipelines.at("quad").info();
  info_pipe2.setShader(this->m_shaders.at("quad"));
  this->m_pipelines.at("quad").recreate(info_pipe2);

  auto info_pipe3 = this->m_pipelines.at("tonemapping").info();
  info_pipe3.setShader(this->m_shaders.at("tonemapping"));
  this->m_pipelines.at("tonemapping").recreate(info_pipe3);


  // auto info_pipe2 = this->m_pipelines.at("lights").info();
  // info_pipe2.setShader(this->m_shaders.at("lights"));
  // this->m_pipelines.at("lights").recreate(info_pipe2);

  auto info_pipe_comp = m_pipeline_compute.info();
  info_pipe_comp.setShader(this->m_shaders.at("compute"));
  m_pipeline_compute.recreate(info_pipe_comp);
}

template<typename T>
void ApplicationScenegraphClustered<T>::createVertexBuffer() {
  vertex_data tri = geometry_loader::obj(this->resourcePath() + "models/sphere.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);
  m_model = Geometry{this->m_transferrer, tri};
}

template<typename T>
void ApplicationScenegraphClustered<T>::createFramebufferAttachments() {
 auto depthFormat = findSupportedFormat(
  this->m_device.physical(),
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );
  auto extent = extent_3d(this->resolution()); 
  this->m_images["depth"] = BackedImage{this->m_device, extent, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment};
  this->m_transferrer.transitionToLayout(this->m_images.at("depth"), vk::ImageLayout::eDepthStencilAttachmentOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("depth"));

  this->m_images["color"] = BackedImage{this->m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  this->m_transferrer.transitionToLayout(this->m_images.at("color"), vk::ImageLayout::eColorAttachmentOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("color"));

  this->m_images["pos"] = BackedImage{this->m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  this->m_transferrer.transitionToLayout(this->m_images.at("pos"), vk::ImageLayout::eColorAttachmentOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("pos"));

  this->m_images["normal"] = BackedImage{this->m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  this->m_transferrer.transitionToLayout(this->m_images.at("normal"), vk::ImageLayout::eColorAttachmentOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("normal"));

  this->m_images["color_2"] = BackedImage{this->m_device, extent, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment};
  this->m_transferrer.transitionToLayout(this->m_images.at("color_2"), vk::ImageLayout::eColorAttachmentOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("color_2"));

  this->m_images["tonemapping_result"] = BackedImage{this->m_device, extent, vk::Format::eB8G8R8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc};
  this->m_transferrer.transitionToLayout(this->m_images.at("tonemapping_result"), vk::ImageLayout::eTransferSrcOptimal);
  this->m_allocators.at("images").allocate(this->m_images.at("tonemapping_result"));

  // update light grid so its extent is computed
  updateLightGrid();
  auto lightVolExtent =
      vk::Extent3D{m_lightGridSize.x, m_lightGridSize.y, m_lightGridSize.z};

  // light volume
  this->m_images["light_vol"] = BackedImage{this->m_device, lightVolExtent, vk::Format::eR32G32B32A32Uint, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage};
  this->m_allocators.at("images").allocate(this->m_images.at("light_vol"));
  this->m_transferrer.transitionToLayout(this->m_images.at("light_vol"), vk::ImageLayout::eGeneral);
}

template<typename T>
void ApplicationScenegraphClustered<T>::updateDescriptors() {
  this->m_images.at("color").view().writeToSet(this->m_descriptor_sets.at("lighting"), 0, vk::DescriptorType::eInputAttachment);
  this->m_images.at("pos").view().writeToSet(this->m_descriptor_sets.at("lighting"), 1, vk::DescriptorType::eInputAttachment);
  this->m_images.at("normal").view().writeToSet(this->m_descriptor_sets.at("lighting"), 2, vk::DescriptorType::eInputAttachment);
  m_instance.dbLight().buffer().writeToSet(this->m_descriptor_sets.at("lighting"), 3, vk::DescriptorType::eStorageBuffer);
  // read
  this->m_images.at("light_vol").view().writeToSet(this->m_descriptor_sets.at("lighting"), 4, m_volumeSampler.get());
  this->m_buffer_views.at("lightgrid").writeToSet(this->m_descriptor_sets.at("lighting"), 5, vk::DescriptorType::eUniformBuffer);

  m_instance.dbCamera().buffer().writeToSet(this->m_descriptor_sets.at("camera"), 0, vk::DescriptorType::eUniformBuffer);
  m_instance.dbCamera().buffer().writeToSet(this->m_descriptor_sets.at("matrix"), 0, vk::DescriptorType::eUniformBuffer);
  m_instance.dbTransform().buffer().writeToSet(this->m_descriptor_sets.at("transform"), 0, vk::DescriptorType::eStorageBuffer);
  m_instance.dbMaterial().buffer().writeToSet(this->m_descriptor_sets.at("material"), 0, vk::DescriptorType::eStorageBuffer);
  m_instance.dbTexture().writeToSet(this->m_descriptor_sets.at("material"), 1, m_instance.dbMaterial().mapping());

  this->m_images.at("color_2").view().writeToSet(this->m_descriptor_sets.at("tonemapping"), 0, vk::DescriptorType::eInputAttachment);

  // read
  m_instance.dbCamera().buffer().writeToSet(this->m_descriptor_sets.at("lightgrid"), 3, vk::DescriptorType::eUniformBuffer);
  m_instance.dbLight().buffer().writeToSet(this->m_descriptor_sets.at("lightgrid"), 2, vk::DescriptorType::eStorageBuffer);
  this->m_buffer_views.at("lightgrid").writeToSet(this->m_descriptor_sets.at("lightgrid"), 1, vk::DescriptorType::eUniformBuffer);
  // write
  this->m_images.at("light_vol").view().writeToSet(this->m_descriptor_sets.at("lightgrid"), 0, vk::DescriptorType::eStorageImage);
}

template<typename T>
void ApplicationScenegraphClustered<T>::createTextureSamplers() {
  m_volumeSampler = Sampler{this->m_device, vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat};
}

template<typename T>
void ApplicationScenegraphClustered<T>::createDescriptorPools() {
  DescriptorPoolInfo info_pool{};
  info_pool.reserve(this->m_shaders.at("scene"), 2);
  // info_pool.reserve(this->m_shaders.at("lights"), 1, 2);
  info_pool.reserve(this->m_shaders.at("compute"), 1);
  info_pool.reserve(this->m_shaders.at("quad"), 1, 2);
  info_pool.reserve(this->m_shaders.at("tonemapping"), 1);

  this->m_descriptor_pool = DescriptorPool{this->m_device, info_pool};
  this->m_descriptor_sets["camera"] = this->m_descriptor_pool.allocate(this->m_shaders.at("scene").setLayout(0));
  this->m_descriptor_sets["transform"] = this->m_descriptor_pool.allocate(this->m_shaders.at("scene").setLayout(1));
  this->m_descriptor_sets["material"] = this->m_descriptor_pool.allocate(this->m_shaders.at("scene").setLayout(2));

  this->m_descriptor_sets["lighting"] = this->m_descriptor_pool.allocate(this->m_shaders.at("quad").setLayout(1));
  // this->m_descriptor_sets["lighting"] = this->m_descriptor_pool.allocate(this->m_shaders.at("lights").setLayout(1));
  this->m_descriptor_sets["matrix"] = this->m_descriptor_pool.allocate(this->m_shaders.at("quad").setLayout(0));

  this->m_descriptor_sets["lightgrid"] = this->m_descriptor_pool.allocate(this->m_shaders.at("compute").setLayout(0));
  this->m_descriptor_sets["tonemapping"] = this->m_descriptor_pool.allocate(this->m_shaders.at("tonemapping").setLayout(0));
}

template<typename T>
void ApplicationScenegraphClustered<T>::createUniformBuffers() {
  this->m_buffers["uniforms"] = Buffer{this->m_device, (sizeof(LightGridBufferObject)) * 2, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst};
  // this->m_buffer_views["light"] = BufferView{sizeof(BufferLights), vk::BufferUsageFlagBits::eStorageBuffer};
  // this->m_buffer_views["uniform"] = BufferView{sizeof(UniformBufferObject), vk::BufferUsageFlagBits::eUniformBuffer};
  this->m_buffer_views["lightgrid"] = BufferView{
      sizeof(LightGridBufferObject), vk::BufferUsageFlagBits::eUniformBuffer};

  this->m_allocators.at("buffers").allocate(this->m_buffers.at("uniforms"));

  this->m_buffer_views.at("lightgrid").bindTo(this->m_buffers.at("uniforms"));
  // this->m_buffer_views.at("light").bindTo(this->m_buffers.at("uniforms"));
  // this->m_buffer_views.at("uniform").bindTo(this->m_buffers.at("uniforms"));
}

template<typename T>
void ApplicationScenegraphClustered<T>::onResize() {
  auto cam = m_instance.dbCamera().get("cam");
  cam.setAspect(float(this->resolution().x) /  float(this->resolution().y));
  m_instance.dbCamera().set("cam", std::move(cam));
}

///////////////////////////// misc functions ////////////////////////////////
template<typename T>
void ApplicationScenegraphClustered<T>::mouseButtonCallback(int button, int action, int mods)
{
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    m_selection_phase_flag = true;
  else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    m_selection_phase_flag = false;
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    endTargetManipulation();
}

///////////////////////////// interaction ////////////////////////////////
template<typename T>
void ApplicationScenegraphClustered<T>::startTargetNavigation()
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

template<typename T>
void ApplicationScenegraphClustered<T>::navigateToTarget()
{
  auto navi = m_graph.findNode("navi_cam");
  glm::vec3 cam_curr_disp = m_cam_new_pos * m_frame_time / float(m_target_navi_duration);
  navi->translate(cam_curr_disp);
}

template<typename T>
void ApplicationScenegraphClustered<T>::manipulate()
{
  auto navi = m_graph.findNode("navi_cam");
  m_curr_hit.getNode()->setLocal(glm::inverse(m_curr_hit.getNode()->getParent()->getWorld()) * navi->getWorld() * m_offset);
}

template<typename T>
void ApplicationScenegraphClustered<T>::endTargetManipulation()
{
  m_fly_phase_flag = true;
  m_selection_phase_flag = false;
  m_target_navi_phase_flag = false;
  m_manipulation_phase_flag = false;
}
