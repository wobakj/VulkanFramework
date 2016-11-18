#include "render_pass.hpp"
#include "device.hpp"

#include "image.hpp"

sub_pass_t::sub_pass_t(std::vector<uint32_t> const& colors, std::vector<uint32_t> const& inputs, int32_t depth) {   
  for(auto const& color : colors) {
    color_refs.emplace_back(vk::AttachmentReference{color, vk::ImageLayout::eColorAttachmentOptimal});
  }
  for(auto const& input : inputs) {
    input_refs.emplace_back(vk::AttachmentReference{input, vk::ImageLayout::eShaderReadOnlyOptimal});
  }
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
  pass.preserveAttachmentCount = std::uint32_t(preserve_refs.size());
  pass.pPreserveAttachments = preserve_refs.data();
  if(depth_ref.layout != vk::ImageLayout::eUndefined) {
    pass.pDepthStencilAttachment = &depth_ref;
  }
  return pass;
}
// indices of references produced by pass
std::vector<vk::AttachmentReference> sub_pass_t::outputs() const {
  std::vector<vk::AttachmentReference> outputs{};
  for(auto const& ref : color_refs) {
    outputs.emplace_back(ref);
  }
  if(depth_ref.layout != vk::ImageLayout::eUndefined) {
    outputs.emplace_back(depth_ref);
  }
  return outputs;
}

// indices of references consumed by pass
std::vector<vk::AttachmentReference> sub_pass_t::inputs() const {
  std::vector<vk::AttachmentReference> inputs = input_refs;
  // write attachments are also inputs
  auto outs = outputs();
  inputs.insert( inputs.end(), outs.begin(), outs.end() );
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
    for(auto const& ref_out : sub_passes[i].outputs()) {
      for(uint32_t j = i + 1; j < sub_passes.size(); ++j) {
        for(auto const& ref_in : sub_passes[j].inputs()) {
          if (ref_in.attachment == ref_out.attachment) {
            dependencies.emplace_back(img_to_dependency(ref_out.layout, ref_in.layout, i, j));
            // add preserve attachments in passes between attachment ouput and usage
            for(uint32_t k = i + 1; k < j; ++k) {
              sub_passes[k].preserve_refs.push_back(ref_in.attachment);
            }
          }
        }
      }
    }
  }
  // add dependency for present image as first stage output 
  for(auto const& ref : sub_passes.front().outputs()) {
    if (images[ref.attachment].initialLayout == vk::ImageLayout::ePresentSrcKHR
     || images[ref.attachment].initialLayout == vk::ImageLayout::eTransferDstOptimal
     || images[ref.attachment].initialLayout == vk::ImageLayout::eTransferSrcOptimal
        ) {
      dependencies.emplace_back(img_to_dependency(images[ref.attachment].initialLayout, ref.layout, VK_SUBPASS_EXTERNAL, 0));
    }
  }
  // add dependency for present image as last stage output 
  for(auto const& ref : sub_passes.back().outputs()) {
    if (images[ref.attachment].initialLayout == vk::ImageLayout::ePresentSrcKHR
     || images[ref.attachment].initialLayout == vk::ImageLayout::eTransferSrcOptimal) {
      dependencies.emplace_back(img_to_dependency(ref.layout, images[ref.attachment].initialLayout, int32_t(sub_passes.size()) - 1, VK_SUBPASS_EXTERNAL));
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

RenderPass::~RenderPass() {
  cleanup();
}

void RenderPass::destroy() {
  (*m_device)->destroyRenderPass(m_object);
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