#include "frame_buffer.hpp"

#include "device.hpp"

vk::FramebufferCreateInfo view_to_fb(vk::ImageView const& view, vk::ImageCreateInfo const& img_info, vk::RenderPass const& pass) {
  vk::ImageView attachments[] = {view};

  vk::FramebufferCreateInfo fb_info{};
  fb_info.renderPass = pass;
  fb_info.attachmentCount = 1;
  fb_info.pAttachments = attachments;
  fb_info.width = img_info.extent.width;
  fb_info.height = img_info.extent.height;
  fb_info.layers = img_info.arrayLayers;
  return fb_info;
}

vk::FramebufferCreateInfo view_to_fb(std::vector<vk::ImageView> const& attachments, vk::ImageCreateInfo const& img_info, vk::RenderPass const& pass) {
  vk::FramebufferCreateInfo fb_info{};
  fb_info.renderPass = pass;
  fb_info.attachmentCount = std::uint32_t(attachments.size());
  fb_info.pAttachments = attachments.data();
  fb_info.width = img_info.extent.width;
  fb_info.height = img_info.extent.height;
  fb_info.layers = img_info.arrayLayers;
  return fb_info;
}

FrameBuffer::FrameBuffer()
 :WrapperFrameBuffer{}
 ,m_device{nullptr}
 ,m_attachments{}
{}

FrameBuffer::FrameBuffer(Device const& device, std::vector<vk::ImageView> const& attachments, vk::ImageCreateInfo const& img_info, vk::RenderPass const& pass)
 :FrameBuffer{}
 {
  m_device = &device;
  m_attachments = attachments;
  m_info = view_to_fb(m_attachments, img_info, pass);
  m_object = (*m_device)->createFramebuffer(info());
 }

void FrameBuffer::destroy() {
  (*m_device)->destroyFramebuffer(get());
}

FrameBuffer::FrameBuffer(FrameBuffer && rhs)
 :FrameBuffer{}
{
  swap(rhs);
}

FrameBuffer::~FrameBuffer() {
  cleanup();
}

FrameBuffer& FrameBuffer::operator=(FrameBuffer&& rhs) {
  swap(rhs);
  return *this;
}

void FrameBuffer::swap(FrameBuffer& rhs) {
  WrapperFrameBuffer::swap(rhs);
  std::swap(m_device, rhs.m_device);
  std::swap(m_attachments, rhs.m_attachments);
}