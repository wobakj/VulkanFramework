#ifndef COMMAND_BUFFER_HPP
#define COMMAND_BUFFER_HPP

#include "wrap/wrapper.hpp"
#include "wrap/pipeline.hpp"

#include <vulkan/vulkan.hpp>

class Device;
class CommandPool;
class Geometry;
class ImageView;
class Buffer;

struct session_t {
  session_t()
  {}
  vk::PipelineBindPoint bind_point;
  vk::PipelineLayout pipe_layout;
  Geometry const* geometry;
};

using WrapperCommandBuffer = Wrapper<vk::CommandBuffer, vk::CommandBufferAllocateInfo>;
class CommandBuffer : public WrapperCommandBuffer {
 public:
  CommandBuffer();
  CommandBuffer(Device const& device, vk::CommandBuffer&& buffer, vk::CommandBufferAllocateInfo const& info);
  CommandBuffer(CommandBuffer && rhs);
  CommandBuffer(CommandBuffer const&) = delete;
  ~CommandBuffer();
  
  CommandBuffer& operator=(CommandBuffer const&) = delete;
  CommandBuffer& operator=(CommandBuffer&& rhs);

  void swap(CommandBuffer& rhs);

  void begin(vk::CommandBufferBeginInfo const& info);
  void end();

  void bindPipeline(Pipeline const& pipeline);
  void bindGeometry(Geometry const& geometry);
 
  template<typename T>
  void pushConstants(vk::ShaderStageFlags stage, uint32_t offset, T const& value) {
    pushConstants(stage, offset, sizeof(value), &value);
  }
  void pushConstants(vk::ShaderStageFlags stage, uint32_t offset, uint32_t size, const void* pValues);

  void bindDescriptorSets(uint32_t first_set, vk::ArrayProxy<const vk::DescriptorSet> sets, vk::ArrayProxy<const uint32_t> dynamic_offsets);

  void copyImage(vk::Image srcImage, vk::ImageLayout srcImageLayout, vk::Image dstImage, vk::ImageLayout dstImageLayout, vk::ArrayProxy<const vk::ImageCopy> regions) const;
  void copyImage(vk::Image srcImage, vk::ImageLayout srcImageLayout, vk::Image dstImage, vk::ImageLayout dstImageLayout, vk::ImageCopy region) const;
  void copyImage(ImageView const& srcImage, vk::ImageLayout srcImageLayout, ImageView const& dstImage, vk::ImageLayout dstImageLayout, vk::ImageCopy region) const;
  void copyImage(ImageView const& srcImage, vk::ImageLayout srcImageLayout, ImageView const& dstImage, vk::ImageLayout dstImageLayout, uint32_t level = 0) const;
  void copyBufferToImage(Buffer const& srcBuffer, ImageView const& dstImage, vk::ImageLayout imageLayout, uint32_t layer = 0) const;
  void copyImageToBuffer(Buffer const& srcBuffer, ImageView const& dstImage, vk::ImageLayout imageLayout, uint32_t layer = 0) const;
  
  void transitionLayout(ImageView const& view, vk::ImageLayout const& layout_old, vk::ImageLayout const& layout_new) const;
  
  void drawGeometry(uint32_t instanceCount = 1, uint32_t firstInstance = 0);
 private:
  CommandBuffer(CommandPool const& pool, uint32_t idx_queue, vk::CommandBufferLevel const& level);
  void destroy() override;

  Device const* m_device;
  bool m_recording;

  session_t m_session;
};

#endif