#include "app/application_worker.hpp"

#include "wrap/surface.hpp"
#include "wrap/conversions.hpp"
#include "wrap/submit_info.hpp"

#include "frame_resource.hpp"

#include "cmdline.h"
//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>

#include <mpi.h>

cmdline::parser ApplicationWorker::getParser() {
  cmdline::parser cmd_parse{Application::getParser()};
  cmd_parse.add("present", 'p', "present mode", false, std::string{"fifo"}, cmdline::oneof<std::string>("fifo", "mailbox", "immediate"));
  return cmd_parse;
}

ApplicationWorker::ApplicationWorker(std::string const& resource_path, Device& device, Surface const& surf, uint32_t image_count, cmdline::parser const& cmd_parse)
 :Application{resource_path, device, image_count - 1, cmd_parse}
 ,m_ptr_buff_transfer{nullptr}
 ,m_should_close{false}
{
  // receive resolution
  MPI::COMM_WORLD.Bcast(&m_resolution, 2, MPI::UNSIGNED, 0);
  std::cout << "resolution " << resolution().x << ", " << resolution().y << std::endl;
  
  createSendBuffer();

  // m_statistics.addTimer("fence_acquire");
  m_statistics.addTimer("present");
}

ApplicationWorker::~ApplicationWorker() {
  // delete buffer before allocator
  m_buffers.erase("transfer");
  std::cout << std::endl;
  std::cout << "Worker" << std::endl;
  // std::cout << "Acquire fence time: " << m_statistics.get("fence_acquire") << " milliseconds" << std::endl;
  std::cout << "Present time: " << m_statistics.get("present") << " milliseconds " << std::endl;
}

void ApplicationWorker::createSendBuffer() {
  // create upload buffer;
  m_buffers["transfer"] = Buffer{m_device, resolution().x * resolution().y * sizeof(glm::u8vec4) * m_frame_resources.size(), vk::BufferUsageFlagBits::eTransferDst};
  
  auto mem_type = this->m_device.findMemoryType(this->m_buffers.at("transfer").requirements().memoryTypeBits 
                              , vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
  m_allocator = StaticAllocator(this->m_device, mem_type, this->m_buffers.at("transfer").requirements().size);
  m_allocator.allocate(this->m_buffers.at("transfer"));

  m_ptr_buff_transfer = m_allocator.map(m_buffers.at("transfer"));
}

void ApplicationWorker::updateFrameResources() {
  for (auto& res : this->m_frame_resources) {
    this->updateResourceDescriptors(res);
    this->updateResourceCommandBuffers(res);
    // update view into transfer buffer
    res.buffer_views["transfer"] = BufferView{resolution().x * resolution().y * sizeof(glm::u8vec4), vk::BufferUsageFlagBits::eTransferDst};
    res.buffer_views.at("transfer").bindTo(this->m_buffers.at("transfer"));
  }
}

void ApplicationWorker::acquireImage(FrameResource& res) {
  //do nothing 
}

void ApplicationWorker::presentCommands(FrameResource& res, ImageLayers const& view, vk::ImageLayout const& layout) {
  res.command_buffers.at("draw").copyImageToBuffer(view, layout, res.buffer_views.at("transfer"));
}

void ApplicationWorker::presentFrame(FrameResource& res) {
  // make sure drawing is done
  this->m_statistics.start("fence_draw");
  res.fence("draw").wait();
  this->m_statistics.stop("fence_draw");
  this->m_statistics.start("present");
  // write data to presenter
  int size = int(res.buffer_views.at("transfer").size());
  MPI::COMM_WORLD.Gather(m_ptr_buff_transfer + res.buffer_views.at("transfer").offset(), size, MPI::BYTE, nullptr, size, MPI::BYTE, 0);
  this->m_statistics.stop("present");
}

void ApplicationWorker::onResize() {
  createSendBuffer();
}

glm::fmat4 const& ApplicationWorker::matrixView() const {
  return m_mat_view;
}

glm::fmat4 const& ApplicationWorker::matrixFrustum() const {
  return m_mat_frustum;
}

void ApplicationWorker::onFrameBegin() {
  // get resolution
  glm::u32vec2 res_new;
  MPI::COMM_WORLD.Bcast((void*)&res_new, 2, MPI::UNSIGNED, 0);
  if (res_new != resolution()) {
    resize(res_new.x, res_new.y);
  }
  // get camera matrices
  // identical view
  MPI::COMM_WORLD.Bcast(&m_mat_view, 16, MPI::FLOAT, 0);

  //different projection for each 
  MPI::COMM_WORLD.Scatter(nullptr, 16, MPI::FLOAT, &m_mat_frustum, 16, MPI::FLOAT, 0);
}

// check if shutdown
void ApplicationWorker::onFrameEnd() {
  uint8_t flag = 0;
  MPI::COMM_WORLD.Bcast(&flag, 1, MPI::BYTE, 0);
  m_should_close = flag > 0;
}

bool ApplicationWorker::shouldClose() const{
  return m_should_close;
}