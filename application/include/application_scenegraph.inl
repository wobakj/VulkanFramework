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

template<typename T>
cmdline::parser ApplicationScenegraph<T>::getParser() {
  return T::getParser();
}

template<typename T>
ApplicationScenegraph<T>::ApplicationScenegraph(std::string const& resource_path, Device& device, Surface const& surf, cmdline::parser const& cmd_parse) 
 :T{resource_path, device, surf, cmd_parse}
 ,m_instance{this->m_device, this->m_command_pools.at("transfer")}
 ,m_model_loader{m_instance}
 ,m_renderer{m_instance}
 ,m_graph{"graph", m_instance}
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
  this->m_shaders.emplace("lights", Shader{this->m_device, {this->resourcePath() + "shaders/lighting_vert.spv", this->resourcePath() + "shaders/deferred_pbr_frag.spv"}});
  this->m_shaders.emplace("tonemapping", Shader{this->m_device, {this->resourcePath() + "shaders/fullscreen_vert.spv", this->resourcePath() + "shaders/tone_mapping_frag.spv"}});

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

  this->createRenderResources();
}

template<typename T>
ApplicationScenegraph<T>::~ApplicationScenegraph() {
  this->shutDown();
}

template<typename T>
Scenegraph& ApplicationScenegraph<T>::getGraph(){
  return m_graph;
}

template<typename T>
FrameResource ApplicationScenegraph<T>::createFrameResource() {
  auto res = T::createFrameResource();
  res.command_buffers.emplace("gbuffer", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("lighting", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  res.command_buffers.emplace("tonemapping", this->m_command_pools.at("graphics").createBuffer(vk::CommandBufferLevel::eSecondary));
  return res;
}

template<typename T>
void ApplicationScenegraph<T>::logic() {
  static double time_last = glfwGetTime();
  // calculate delta time
  double time_current = glfwGetTime();
  m_frame_time = float(time_current - time_last);
  time_last = time_current;

  // m_graph.getRoot()->getChild("sphere")->rotate(m_frame_time, glm::fvec3{0.0f, 1.0f, 0.0f});

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
void ApplicationScenegraph<T>::updateResourceCommandBuffers(FrameResource& res) {
  res.command_buffers.at("gbuffer")->reset({});

  vk::CommandBufferInheritanceInfo inheritanceInfo{};
  inheritanceInfo.renderPass = m_render_pass;
  inheritanceInfo.framebuffer = m_framebuffer;
  inheritanceInfo.subpass = 0;

  // first pass
  res.command_buffers.at("gbuffer").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("gbuffer").bindPipeline(this->m_pipelines.at("scene"));
  res.command_buffers.at("gbuffer").bindDescriptorSets(0, {this->m_descriptor_sets.at("camera"), this->m_descriptor_sets.at("transform"), this->m_descriptor_sets.at("material")}, {});
  res.command_buffers.at("gbuffer")->setViewport(0, viewport(this->resolution()));
  res.command_buffers.at("gbuffer")->setScissor(0, rect(this->resolution()));

  RenderVisitor render_visitor{};
  m_graph.accept(render_visitor);
  // draw collected models
  m_renderer.draw(res.command_buffers.at("gbuffer"), render_visitor.visibleNodes());

  res.command_buffers.at("gbuffer").end();
  //deferred shading pass 
  inheritanceInfo.subpass = 1;
  res.command_buffers.at("lighting")->reset({});
  res.command_buffers.at("lighting").begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  // barrier to make light data visible to vertex shader
  res.command_buffers.at("lighting").bufferBarrier(m_instance.dbLight().buffer(), 
    vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite, 
    vk::PipelineStageFlagBits::eVertexShader, vk::AccessFlagBits::eShaderRead
  );
  
  res.command_buffers.at("lighting").bindPipeline(this->m_pipelines.at("lights"));
  res.command_buffers.at("lighting")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("lights").layout(), 0, {this->m_descriptor_sets.at("matrix"), this->m_descriptor_sets.at("lighting")}, {});
  res.command_buffers.at("lighting")->setViewport(0, viewport(this->resolution()));
  res.command_buffers.at("lighting")->setScissor(0, rect(this->resolution()));

  res.command_buffers.at("lighting").bindGeometry(m_model);

  res.command_buffers.at("lighting").drawGeometry(uint32_t(m_instance.dbLight().size()));

  res.command_buffers.at("lighting").end();

  // tonemapping
  inheritanceInfo.subpass = 2;
  res.command_buffers.at("tonemapping")->reset({});
  res.command_buffers.at("tonemapping")->begin({vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse, &inheritanceInfo});

  res.command_buffers.at("tonemapping")->bindPipeline(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("tonemapping"));
  res.command_buffers.at("tonemapping")->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, this->m_pipelines.at("tonemapping").layout(), 0, {this->m_descriptor_sets.at("tonemapping")}, {});
  res.command_buffers.at("tonemapping")->setViewport(0, viewport(this->resolution()));
  res.command_buffers.at("tonemapping")->setScissor(0, rect(this->resolution()));

  res.command_buffers.at("tonemapping")->draw(3, 1, 0, 0);

  res.command_buffers.at("tonemapping")->end();
}

template<typename T>
void ApplicationScenegraph<T>::recordDrawBuffer(FrameResource& res) {
  res.command_buffers.at("draw")->reset({});

  res.command_buffers.at("draw")->begin({vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  // copy transform data
  m_instance.dbLight().updateCommand(res.command_buffers.at("draw"));
  m_instance.dbTransform().updateCommand(res.command_buffers.at("draw"));
  m_instance.dbCamera().updateCommand(res.command_buffers.at("draw"));

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

  this->presentCommands(res, this->m_images.at("tonemapping_result"), vk::ImageLayout::eTransferSrcOptimal);

  res.command_buffers.at("draw")->end();
}

template<typename T>
void ApplicationScenegraph<T>::createFramebuffers() {
  m_framebuffer = FrameBuffer{this->m_device, {&this->m_images.at("color"), &this->m_images.at("pos"), &this->m_images.at("normal"), &this->m_images.at("depth"), &this->m_images.at("color_2"), &this->m_images.at("tonemapping_result")}, m_render_pass};
}

template<typename T>
void ApplicationScenegraph<T>::createRenderPasses() {
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
void ApplicationScenegraph<T>::createPipelines() {
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
  info_pipe2.setTopology(vk::PrimitiveTopology::eTriangleList);
  
  vk::PipelineRasterizationStateCreateInfo rasterizer2{};
  rasterizer2.lineWidth = 1.0f;
  rasterizer2.cullMode = vk::CullModeFlagBits::eFront;
  info_pipe2.setRasterizer(rasterizer2);

  vk::PipelineColorBlendAttachmentState colorBlendAttachment2{};
  colorBlendAttachment2.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment2.blendEnable = VK_TRUE;
  colorBlendAttachment2.srcColorBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment2.dstColorBlendFactor = vk::BlendFactor::eOne;
  colorBlendAttachment2.colorBlendOp = vk::BlendOp::eAdd;
  info_pipe2.setAttachmentBlending(colorBlendAttachment2, 0);

  info_pipe2.setVertexInput(m_model.vertexInfo());
  info_pipe2.setShader(this->m_shaders.at("lights"));
  info_pipe2.setPass(m_render_pass, 1);
  info_pipe2.addDynamic(vk::DynamicState::eViewport);
  info_pipe2.addDynamic(vk::DynamicState::eScissor);

  vk::PipelineDepthStencilStateCreateInfo depthStencil2{};
  depthStencil2.depthTestEnable = VK_TRUE;
  depthStencil2.depthWriteEnable = VK_FALSE;
  depthStencil2.depthCompareOp = vk::CompareOp::eGreater;
  info_pipe2.setDepthStencil(depthStencil2);

  // pipeline for tonemapping
  info_pipe3.setResolution(extent_2d(this->resolution()));
  info_pipe3.setTopology(vk::PrimitiveTopology::eTriangleStrip);

  info_pipe3.setRasterizer(rasterizer2);

  info_pipe3.setAttachmentBlending(colorBlendAttachment, 0);

  info_pipe3.setShader(this->m_shaders.at("tonemapping"));
  info_pipe3.setPass(m_render_pass, 2);
  info_pipe3.addDynamic(vk::DynamicState::eViewport);
  info_pipe3.addDynamic(vk::DynamicState::eScissor);

  this->m_pipelines.emplace("scene", GraphicsPipeline{this->m_device, info_pipe, this->m_pipeline_cache});
  this->m_pipelines.emplace("lights", GraphicsPipeline{this->m_device, info_pipe2, this->m_pipeline_cache});
  this->m_pipelines.emplace("tonemapping", GraphicsPipeline{this->m_device, info_pipe3, this->m_pipeline_cache});
}

template<typename T>
void ApplicationScenegraph<T>::updatePipelines() {
  auto info_pipe = this->m_pipelines.at("scene").info();
  info_pipe.setShader(this->m_shaders.at("scene"));
  this->m_pipelines.at("scene").recreate(info_pipe);

  auto info_pipe2 = this->m_pipelines.at("lights").info();
  info_pipe2.setShader(this->m_shaders.at("lights"));
  this->m_pipelines.at("lights").recreate(info_pipe2);
}

template<typename T>
void ApplicationScenegraph<T>::createVertexBuffer() {
  vertex_data tri = geometry_loader::obj(this->resourcePath() + "models/sphere.obj", vertex_data::NORMAL | vertex_data::TEXCOORD);
  m_model = Geometry{this->m_transferrer, tri};
}

template<typename T>
void ApplicationScenegraph<T>::createFramebufferAttachments() {
 auto depthFormat = findSupportedFormat(
  this->m_device.physical(),
    {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
    vk::ImageTiling::eOptimal,
    vk::FormatFeatureFlagBits::eDepthStencilAttachment
  );
  auto extent = extent_3d(extent_2d(this->resolution())); 
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
}

template<typename T>
void ApplicationScenegraph<T>::updateDescriptors() {
  this->m_images.at("color").view().writeToSet(this->m_descriptor_sets.at("lighting"), 0, vk::DescriptorType::eInputAttachment);
  this->m_images.at("pos").view().writeToSet(this->m_descriptor_sets.at("lighting"), 1, vk::DescriptorType::eInputAttachment);
  this->m_images.at("normal").view().writeToSet(this->m_descriptor_sets.at("lighting"), 2, vk::DescriptorType::eInputAttachment);
  m_instance.dbLight().buffer().writeToSet(this->m_descriptor_sets.at("lighting"), 3, vk::DescriptorType::eStorageBuffer);

  m_instance.dbCamera().buffer().writeToSet(this->m_descriptor_sets.at("camera"), 0, vk::DescriptorType::eUniformBuffer);
  m_instance.dbCamera().buffer().writeToSet(this->m_descriptor_sets.at("matrix"), 0, vk::DescriptorType::eUniformBuffer);
  m_instance.dbTransform().buffer().writeToSet(this->m_descriptor_sets.at("transform"), 0, vk::DescriptorType::eStorageBuffer);
  m_instance.dbMaterial().buffer().writeToSet(this->m_descriptor_sets.at("material"), 0, vk::DescriptorType::eStorageBuffer);
  m_instance.dbTexture().writeToSet(this->m_descriptor_sets.at("material"), 1, m_instance.dbMaterial().mapping());

  this->m_images.at("color_2").view().writeToSet(this->m_descriptor_sets.at("tonemapping"), 0, vk::DescriptorType::eInputAttachment);
}

template<typename T>
void ApplicationScenegraph<T>::createDescriptorPools() {
  DescriptorPoolInfo info_pool{};
  info_pool.reserve(this->m_shaders.at("scene"), 2);
  info_pool.reserve(this->m_shaders.at("lights"), 1, 2);

  this->m_descriptor_pool = DescriptorPool{this->m_device, info_pool};
  this->m_descriptor_sets["camera"] = this->m_descriptor_pool.allocate(this->m_shaders.at("scene").setLayout(0));
  this->m_descriptor_sets["transform"] = this->m_descriptor_pool.allocate(this->m_shaders.at("scene").setLayout(1));
  this->m_descriptor_sets["material"] = this->m_descriptor_pool.allocate(this->m_shaders.at("scene").setLayout(2));

  this->m_descriptor_sets["lighting"] = this->m_descriptor_pool.allocate(this->m_shaders.at("lights").setLayout(1));
  this->m_descriptor_sets["matrix"] = this->m_descriptor_pool.allocate(this->m_shaders.at("lights").setLayout(0));

  this->m_descriptor_sets["tonemapping"] = this->m_descriptor_pool.allocate(this->m_shaders.at("tonemapping").setLayout(0));
}

template<typename T>
void ApplicationScenegraph<T>::onResize() {
  auto cam = m_instance.dbCamera().get("cam");
  cam.setAspect(float(this->resolution().x) / float(this->resolution().x));
  m_instance.dbCamera().set("cam", std::move(cam));
}

/////////////////////////////misc functions ////////////////////////////////
template<typename T> 
void ApplicationScenegraph<T>::mouseButtonCallback(int button, int action, int mods)
{
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    m_selection_phase_flag = true;
  else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
    m_selection_phase_flag = false;
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    endTargetManipulation();
}

template<typename T>
void ApplicationScenegraph<T>::startTargetNavigation()
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

  m_cam_new_pos = glm::vec3(ray_world->worldRay().direction().x, ray_world->worldRay().direction().y, ray_world->worldRay().direction().z) * (m_curr_hit.dist() * 0.3f);
  std::cout<<"dist" << m_curr_hit.dist()<<std::endl;
  std::cout<<"cam old pos " << m_cam_new_pos.x << " " << m_cam_new_pos.y << " " << m_cam_new_pos.z << std::endl;
}

template<typename T>
void ApplicationScenegraph<T>::navigateToTarget()
{
  auto navi = m_graph.findNode("navi_cam");
  glm::vec3 cam_curr_disp = m_cam_new_pos * m_frame_time / float(m_target_navi_duration);
  navi->translate(cam_curr_disp);
}

template<typename T>
void ApplicationScenegraph<T>::manipulate()
{
  auto navi = m_graph.findNode("navi_cam");
  m_curr_hit.getNode()->setLocal(glm::inverse(m_curr_hit.getNode()->getParent()->getWorld()) * navi->getWorld() * m_offset);
}

template<typename T>
void ApplicationScenegraph<T>::endTargetManipulation()
{
  m_fly_phase_flag = true;
  m_selection_phase_flag = false;
  m_target_navi_phase_flag = false;
  m_manipulation_phase_flag = false;
}