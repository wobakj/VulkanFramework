#include "application_threaded.hpp"

#include "launcher.hpp"
#include "image.hpp"
#include "buffer.hpp"
#include "shader.hpp"
#include "texture_loader.hpp"
#include "model_loader.hpp"

// c++ warpper
#include <vulkan/vulkan.hpp>

#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <iostream>
#include <chrono>

ApplicationThreaded::ApplicationThreaded(std::string const& resource_path, Device& device, SwapChain const& chain, GLFWwindow* window) 
 :Application{resource_path, device, chain, window}
 ,m_should_draw{true}
 ,m_semaphore_draw{0}
 ,m_semaphore_record{m_swap_chain.numImages() - 1}
{
  if (!m_thread_render.joinable()) {
    m_thread_render = std::thread(&ApplicationThreaded::drawLoop, this);
  }
}

void ApplicationThreaded::shut_down() {
  // shut down render thread
  m_should_draw = false;
  if(m_thread_render.joinable()) {
    m_thread_render.join();
  }
  else {
    throw std::runtime_error{"could not join thread"};
  }
  // wait until fences are done
  for (auto const& res : m_frame_resources) {
    res.waitFences();
  }

}
// void ApplicationThreadedSimple::createFrameResources() {
//   // create resources for one less image than swap chain
//   // only numImages - 1 images can be acquired at a time
//   for (uint32_t i = 0; i < m_swap_chain.numImages() - 1; ++i) {
//     m_frame_resources.emplace_back(m_device);
//     auto& res = m_frame_resources.back();
//     createCommandBuffers(res);
//     m_queue_record_frames.push(i);
//     res.buffer_views["uniform"] = BufferView{sizeof(UniformBufferObject)};
//     res.buffer_views.at("uniform").bindTo(m_buffers.at("uniforms"));
//   }
// }

void ApplicationThreaded::render() {
  // if(m_model_dirty.is_lock_free()) {
  //   if(m_model_dirty) {
  //     updateModel();
  //   }
  // }
  m_semaphore_record.wait();
  static uint64_t frame = 0;
  ++frame;
  // get next frame to record
  uint32_t frame_record = 0;
  // only calculate new frame if previous one was rendered
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_record_queue};
    assert(!m_queue_record_frames.empty());
    // get next frame to record
    frame_record = m_queue_record_frames.front();
    m_queue_record_frames.pop();
  }
  auto& resource_record = m_frame_resources.at(frame_record);

  // wait for last acquisition until acquiring again
  resource_record.fenceAcquire().wait();
  resource_record.fenceAcquire().reset();
  acquireImage(resource_record);
  // wait for drawing finish until rerecording
  resource_record.fenceDraw().wait();
  recordDrawBuffer(resource_record);

  // add newly recorded frame for drawing
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_draw_queue};
    m_queue_draw_frames.push(frame_record);
    m_semaphore_draw.signal();
  }
  // std::this_thread::sleep_for(std::chrono::milliseconds{10}); 
}

void ApplicationThreaded::draw() {
  m_semaphore_draw.wait();
  static std::uint64_t frame = 0;
  ++frame;
  uint32_t frame_draw = 0;
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_draw_queue};
    assert(!m_queue_draw_frames.empty());
    // get frame to draw
    frame_draw = m_queue_draw_frames.front();
    m_queue_draw_frames.pop();
  }
  auto& resource_draw = m_frame_resources.at(frame_draw);
  resource_draw.fenceDraw().reset();
  submitDraw(resource_draw);
  // present image and wait for result
  present(resource_draw);
  // make frame avaible for rerecording
  {
    std::lock_guard<std::mutex> queue_lock{m_mutex_record_queue};
    m_queue_record_frames.push(frame_draw);
    m_semaphore_record.signal();
  }
}

void ApplicationThreaded::drawLoop() {
  while (m_should_draw) {
    draw();
  }
}

void ApplicationThreaded::emptyDrawQueue() {
  // render remaining recorded frames
  bool all_frames_drawn = false;
  while(!all_frames_drawn) {
    // check if all frames are ready for recording
    {
      std::lock_guard<std::mutex> queue_lock{m_mutex_record_queue};
      all_frames_drawn = m_queue_record_frames.size() == m_frame_resources.size();
    }  
    // if draw frames remain, wait until next was drawn
    if (!all_frames_drawn) {
      m_semaphore_record.wait(); 
    }
  }
  // wait until draw resources are avaible before recallocation
  for (auto const& res : m_frame_resources) {
    res.waitFences();
  }
  // give record queue enough signals to process all frames
  m_semaphore_record.set(uint32_t(m_frame_resources.size()));
}

void ApplicationThreaded::resize() {
  // draw queue is emptied in launcher::resize
  createFramebufferAttachments();
  createRenderPasses();
  createFramebuffers();

  createPipelines();
  createDescriptorPools();
  for (auto& res : m_frame_resources) {
    updateDescriptors(res);
    updateCommandBuffers(res);
  }
}

void ApplicationThreaded::recreatePipeline() {
  emptyDrawQueue();

  createPipelines();
  createDescriptorPools();
  for (auto& res : m_frame_resources) {
    updateDescriptors(res);
    updateCommandBuffers(res);
  }
}