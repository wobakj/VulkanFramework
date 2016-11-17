#include "render_pass.hpp"
#include "device.hpp"

#include "image.hpp"

sub_pass_t::sub_pass_t(std::vector<uint32_t> const& colors, std::vector<uint32_t> const& inputs, int32_t depth, std::vector<uint32_t> const& preserves) {   
  for(auto const& color : colors) {
    color_refs.emplace_back(vk::AttachmentReference{color, vk::ImageLayout::eColorAttachmentOptimal});
  }
  for(auto const& input : inputs) {
    input_refs.emplace_back(vk::AttachmentReference{input, vk::ImageLayout::eShaderReadOnlyOptimal});
  }
  preserve_refs = preserves;
  if(depth >= 0) {
    depth_ref = vk::AttachmentReference{uint32_t(depth), vk::ImageLayout::eDepthStencilAttachmentOptimal};
  }
}

vk::SubpassDescription sub_pass_t::to_description() const {
  vk::SubpassDescription pass{};
  pass.colorAttachmentCount = std::uint32_t(color_refs.size());
  pass.pColorAttachments = color_refs.data();
  pass.inputAttachmentCount = std::uint32_t(input_refs.size());
  pass.pInputAttachments = input_refs.data();
  if(depth_ref.layout != vk::ImageLayout::eUndefined) {
    pass.pDepthStencilAttachment = &depth_ref;
  }
  return pass;
}
// indices of references produced by pass
std::vector<uint32_t> sub_pass_t::outputs() const {
  std::vector<uint32_t> outputs{};
  for(auto const& ref : color_refs) {
    outputs.emplace_back(ref.attachment);
  }
  if(depth_ref.layout != vk::ImageLayout::eUndefined) {
    outputs.emplace_back(depth_ref.attachment);
  }
  return outputs;
}

// indices of references consumed by pass
std::unordered_set<uint32_t> sub_pass_t::inputs() const {
  std::unordered_set<uint32_t> inputs{};
  for(auto const& ref : input_refs) {
    inputs.emplace(ref.attachment);
  }
  for(auto const& ref : preserve_refs) {
    inputs.emplace(ref);
  }
  return inputs;
}

render_pass_t::render_pass_t() 
{}

render_pass_t::render_pass_t(std::vector<vk::ImageCreateInfo> const& images, std::vector<sub_pass_t> const& subpasses)
 :sub_passes{subpasses}
 ,attachments{}
 {
  for(std::size_t i = 0; i < images.size(); ++i) {
    attachments.emplace_back(img_to_attachment(images[i], true));
  }
  // do not use subpass input parameter vector, will break with initializer list
  for(uint32_t i = 0; i < sub_passes.size(); ++i) {
    sub_descriptions.emplace_back(sub_passes[i].to_description());
    // check for each output, if it is consumed by a later pass 
    for(uint32_t ref : sub_passes[i].outputs()) {
      for(uint32_t j = i + 1; j < sub_passes.size(); ++j) {
        auto inputs = sub_passes[j].inputs();
        if (inputs.find(ref) != inputs.end()) {
          dependencies.emplace_back(img_to_dependency(images[i], i, j));
        }
      }
    }
  }
  // add dependency for present image as first stage output 
  for(uint32_t ref : sub_passes.front().outputs()) {
    if (images[ref].initialLayout == vk::ImageLayout::ePresentSrcKHR) {
      dependencies.emplace_back(img_to_dependency(images[ref], -1, 0));
    }
  }
  // add dependency for present image as last stage output 
  for(uint32_t ref : sub_passes.back().outputs()) {
    if (images[ref].initialLayout == vk::ImageLayout::ePresentSrcKHR) {
      dependencies.emplace_back(img_to_dependency(images[ref], int32_t(sub_passes.size()) -1, -1));
    }
  }
}

vk::RenderPassCreateInfo render_pass_t::to_info() const {
  vk::RenderPassCreateInfo pass_info{};
  pass_info.attachmentCount = std::uint32_t(attachments.size());
  pass_info.pAttachments = attachments.data();
  pass_info.subpassCount = std::uint32_t(sub_descriptions.size());
  pass_info.pSubpasses = sub_descriptions.data();
  pass_info.dependencyCount = std::uint32_t(dependencies.size());
  pass_info.pDependencies = dependencies.data();

  return pass_info;
}

RenderPass::RenderPass()
{}

RenderPass::RenderPass(Device const& device, std::vector<vk::ImageCreateInfo> const& images, std::vector<sub_pass_t> const& subpasses)
 :WrapperRenderPass{}
 ,m_device{&device}
{
  m_info = render_pass_t{images, subpasses};
  m_object =device->createRenderPass(m_info.to_info(), nullptr);
}

void RenderPass::destroy() {
  (*m_device)->destroyRenderPass(get());
}

RenderPass::RenderPass(RenderPass && dev)
 :RenderPass{}
{
  swap(dev);
}

RenderPass& RenderPass::operator=(RenderPass&& dev) {
  swap(dev);
  return *this;
}

void RenderPass::swap(RenderPass& dev) {
  WrapperRenderPass::swap(dev);
  std::swap(m_device, dev.m_device);
}