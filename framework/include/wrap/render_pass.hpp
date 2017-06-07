#ifndef RENDER_PASS_HPP
#define RENDER_PASS_HPP

#include "wrap/wrapper.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <unordered_set>

class Device;

vk::SubpassDependency img_to_dependency(vk::ImageLayout const& src_layout, vk::ImageLayout const& dst_layout, uint32_t src_pass, uint32_t dst_pass);
vk::AttachmentDescription img_to_attachment(vk::ImageCreateInfo const& img_info, bool clear = true);


struct sub_pass_t {
 public:
  void setColorAttachment(size_t i, uint32_t index);
  void setDepthAttachment(uint32_t index);
  void setInputAttachment(size_t i, uint32_t index);
  // attachments used as sampler in this stage must be referenced a spreserve to create dependency
  sub_pass_t();
  sub_pass_t(std::vector<uint32_t> const& colors, std::vector<uint32_t> const& inputs = std::vector<uint32_t>{}, int32_t depth = -1);

  vk::SubpassDescription to_description() const;
  // indices of references produced by pass
  std::vector<vk::AttachmentReference> outputs() const;

  // indices of references consumed by pass
  std::vector<vk::AttachmentReference> inputs() const;
 private:
  std::vector<vk::AttachmentReference> color_refs;
  vk::AttachmentReference depth_ref;
  std::vector<vk::AttachmentReference> input_refs;
  std::vector<uint32_t> preserve_refs;

  friend class render_pass_t;
};

struct render_pass_t {
  render_pass_t();
  // dependencies for first pass inputs not supported
  render_pass_t(std::vector<vk::ImageCreateInfo> const& images, std::vector<sub_pass_t> const& subpasses);

  vk::RenderPassCreateInfo to_info() const;

  std::vector<sub_pass_t> sub_passes;
  std::vector<vk::AttachmentDescription> attachments;
  std::vector<vk::SubpassDescription> sub_descriptions;
  std::vector<vk::SubpassDependency> dependencies;
};

using WrapperRenderPass = Wrapper<vk::RenderPass, render_pass_t>;
class RenderPass : public WrapperRenderPass {
 public:
  RenderPass();
  RenderPass(Device const& device, std::vector<vk::ImageCreateInfo> const& images, std::vector<sub_pass_t> const& subpasses);
  RenderPass(RenderPass && dev);
  RenderPass(RenderPass const&) = delete;
  ~RenderPass();

  RenderPass& operator=(RenderPass const&) = delete;
  RenderPass& operator=(RenderPass&& dev);

  void swap(RenderPass& dev);

 private:
  void destroy() override;

  Device const* m_device;
};

#endif