#include "app/application_worker.hpp"

#include "wrap/surface.hpp"

#include "frame_resource.hpp"
//dont load gl bindings from glfw
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>

#include <mpi.h>

cmdline::parser ApplicationWorker::getParser() {
  cmdline::parser cmd_parse{Application::getParser()};
  // cmd_parse.add("present", 'p', "present mode", false, std::string{"fifo"}, cmdline::oneof<std::string>("fifo", "mailbox", "immediate"));
  return cmd_parse;
}

ApplicationWorker::ApplicationWorker(std::string const& resource_path, Device& device, Surface const& surf, uint32_t image_count, cmdline::parser const& cmd_parse)
 :Application{resource_path, device, cmd_parse}
 // ,m_camera{45.0f, 1.0f, 0.1f, 500.0f, &surf.window()}
 ,m_ptr_buff_transfer{nullptr}
 ,m_should_close{false}
{
  m_resolution = glm::uvec2{1280, 720};
  // m_images_draw.resize(image_count);
  createImages(image_count);
  createTransferBuffer();

  m_statistics.addTimer("fence_acquire");
  m_statistics.addTimer("queue_present");
}

ApplicationWorker::~ApplicationWorker() {
  m_buffers.at("transfer").unmap();
  std::cout << std::endl;
  std::cout << "Average acquire fence time: " << m_statistics.get("fence_acquire") << " milliseconds" << std::endl;
  std::cout << "Average present queue time: " << m_statistics.get("queue_present") << " milliseconds " << std::endl;
}

FrameResource ApplicationWorker::createFrameResource() {
  auto res = Application::createFrameResource();
  return res;
}

void ApplicationWorker::createImages(uint32_t image_count) {

  for(uint32_t i = 0; i< image_count; ++i) {
    m_images_draw.emplace_back(m_device, vk::Extent3D{m_resolution.x, m_resolution.y, 1}, vk::Format::eB8G8R8A8Unorm, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferSrc
                                                                                                                          | vk::ImageUsageFlagBits::eTransferDst
                                                                                                                          | vk::ImageUsageFlagBits::eColorAttachment);
    m_allocators.at("images").allocate(m_images_draw.at(i));
    m_transferrer.transitionToLayout(m_images_draw.at(i), vk::ImageLayout::eColorAttachmentOptimal);
    m_queue_images.push(uint32_t(m_queue_images.size()));
  }
}

void ApplicationWorker::createTransferBuffer() {
  // create upload buffer;
  m_buffers["transfer"] = Buffer{m_device, m_images_draw.front().size(), vk::BufferUsageFlagBits::eTransferDst};
  m_memory_transfer = Memory{m_device, m_buffers.at("transfer").memoryTypeBits(), vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, m_buffers.at("transfer").size()};
  m_buffers.at("transfer").bindTo(m_memory_transfer, 0);

  auto extent = m_images_draw.front().extent();
  std::vector<glm::u8vec4> color_data{extent.width * extent.height, glm::u8vec4{0, 255, 0, 255}};
  for(unsigned x = 0; x < extent.width; ++x) {
    for(unsigned y = 0; y < extent.height; ++y) {
      color_data[y * extent.width + x] = glm::u8vec4{0, uint8_t(float(y) / float(extent.height) * 255), uint8_t(float(x) / float(extent.width) * 255), 255};
    }
  }
  std::cout << "vector " << color_data.size() * sizeof(color_data.front()) << ",  buffer " << m_buffers.at("transfer").size() << std::endl;
  m_buffers.at("transfer").setData(color_data.data(), color_data.size() * sizeof(color_data.front()), 0);

  m_ptr_buff_transfer = m_buffers.at("transfer").map();

}

void ApplicationWorker::acquireImage(FrameResource& res) {
  uint32_t index = pullImageToDraw();
  res.image = index;
  res.target_view = &m_images_draw.at(index).view();
}

void ApplicationWorker::presentFrame(FrameResource& res) {
  res.fence("draw").wait();
  m_transferrer.transitionToLayout(*res.target_view, vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferSrcOptimal);
  m_transferrer.copyImageToBuffer(m_buffers.at("transfer"), *res.target_view, vk::ImageLayout::eTransferSrcOptimal);
  // write data to presenter
  MPI::COMM_WORLD.Gather(m_ptr_buff_transfer, int(m_buffers.at("transfer").size()), MPI_UNSIGNED_CHAR, nullptr, int(m_buffers.at("transfer").size()), MPI_UNSIGNED_CHAR, 1);
  pushImageToDraw(res.image);
}

void ApplicationWorker::resize(std::size_t width, std::size_t height) {
  // m_swap_chain.recreate(vk::Extent2D{uint32_t(width), uint32_t(height)});
  // m_camera.setAspect(float(width) / float(height));

  Application::resize(width, height);
}

void ApplicationWorker::pushImageToDraw(uint32_t frame) {
  m_queue_images.push(frame);
}

uint32_t ApplicationWorker::pullImageToDraw() {
  assert(!m_queue_images.empty());
  // get frame to draw
  uint32_t frame_draw = m_queue_images.front();
  m_queue_images.pop();
  return frame_draw;
}

glm::fmat4 const& ApplicationWorker::matrixView() const {
  return m_mat_view;
}

glm::fmat4 const& ApplicationWorker::matrixFrustum() const {
  return m_mat_frustum;
}

void ApplicationWorker::onFrameBegin() {
  // get camera matrices
  // identical view
  MPI::COMM_WORLD.Bcast(&m_mat_view, 16, MPI_FLOAT, 1);
  //different projection for each 
  MPI::COMM_WORLD.Scatter(nullptr, 16, MPI_FLOAT, &m_mat_frustum, 16, MPI_FLOAT, 1);
}

// check if shutdown
void ApplicationWorker::onFrameEnd() {
  uint8_t flag = 0;
  MPI::COMM_WORLD.Bcast(&flag, 1, MPI_BYTE, 1);
  m_should_close = flag > 0;
}

bool ApplicationWorker::shouldClose() const{
  return m_should_close;
}