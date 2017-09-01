#include "wrap/frame_buffer.hpp"

#include "wrap/device.hpp"
#include "wrap/image.hpp"

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
{}

FrameBuffer::FrameBuffer(Device const& device, std::vector<Image const*> const& images, vk::RenderPass const& pass)
 :FrameBuffer{}
 {
  m_device = device;
  std::vector<vk::ImageView> attachments;
  for (auto const& image : images) {
    attachments.emplace_back(image->view());
  }
  m_info = view_to_fb(attachments, images.front()->info(), pass);
  m_object = m_device.createFramebuffer(info());

  for(auto const& image : images) {
    if (!is_depth(image->view().format())) {
      m_clear_values.emplace_back(vk::ClearColorValue{std::array<float,4>{0.0f, 0.0f, 0.0f, 1.0f}});
    }
    else {
      m_clear_values.emplace_back(vk::ClearDepthStencilValue{{1.0f, 0}});
    }
  }
 }

void FrameBuffer::destroy() {
  m_device.destroyFramebuffer(get());
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
  std::swap(m_clear_values, rhs.m_clear_values);
}

vk::Extent2D FrameBuffer::extent() const {
  return vk::Extent2D{info().width, info().height};
}

vk::RenderPassBeginInfo FrameBuffer::beginInfo() const {
  vk::RenderPassBeginInfo begin_info{};
  begin_info.renderPass = info().renderPass;
  begin_info.framebuffer = get();
  begin_info.renderArea.extent = extent();
  begin_info.clearValueCount = std::uint32_t(m_clear_values.size());
  begin_info.pClearValues = m_clear_values.data();
  return begin_info;
}