#ifndef FRAME_BUFFER_HPP
#define FRAME_BUFFER_HPP

#include "wrapper.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>

class Device;

vk::FramebufferCreateInfo view_to_fb(vk::ImageView const& view, vk::ImageCreateInfo const& img_info, vk::RenderPass const& pass);
vk::FramebufferCreateInfo view_to_fb(std::vector<vk::ImageView> const& attachments, vk::ImageCreateInfo const& img_info, vk::RenderPass const& pass);

using WrapperFrameBuffer = Wrapper<vk::Framebuffer, vk::FramebufferCreateInfo>;
class FrameBuffer : public WrapperFrameBuffer {
 public:
  FrameBuffer();
  FrameBuffer(Device const& device, std::vector<vk::ImageView> const& attachments, vk::ImageCreateInfo const& img_info, vk::RenderPass const& pass);
  FrameBuffer(FrameBuffer && dev);
  FrameBuffer(FrameBuffer const&) = delete;
  ~FrameBuffer();

  FrameBuffer& operator=(FrameBuffer const&) = delete;
  FrameBuffer& operator=(FrameBuffer&& dev);

  void swap(FrameBuffer& dev);

 private:
  void destroy() override;

  Device const* m_device;
  std::vector<vk::ImageView> m_attachments;
};

#endif