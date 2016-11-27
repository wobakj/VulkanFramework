#include "render_pass.hpp"
#include "device.hpp"

// #include "image.hpp"

vk::SubpassDependency img_to_dependency(vk::ImageLayout const& src_layout, vk::ImageLayout const& dst_layout, uint32_t src_pass, uint32_t dst_pass) {
  vk::SubpassDependency dependency{};
  dependency.srcSubpass = src_pass;
  dependency.dstSubpass = dst_pass;

  // write to transfer dst should be done
  if (src_layout == vk::ImageLayout::eTransferDstOptimal) {
    dependency.srcStageMask = vk::PipelineStageFlagBits::eTransfer;
    dependency.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
  }
  // read from transfer source should be done
  else if (src_layout == vk::ImageLayout::eTransferSrcOptimal) {
    dependency.srcStageMask = vk::PipelineStageFlagBits::eTransfer;
    dependency.srcAccessMask = vk::AccessFlagBits::eTransferRead;
  }
  else if (src_layout == vk::ImageLayout::eColorAttachmentOptimal) {
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
  }
  else if (src_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
  }
  // presentation before renderpass, taken from
  // https://github.com/GameTechDev/IntroductionToVulkan/blob/master/Project/Tutorial04/Tutorial04.cpp
  else if (src_layout == vk::ImageLayout::ePresentSrcKHR) {
    dependency.srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
    dependency.srcAccessMask = vk::AccessFlagBits::eMemoryRead;
  }
  else {
    throw std::runtime_error{"source layout" + to_string(src_layout) + " not supported"};
  } 

  // color input attachment can only be read in frag shader
  if (dst_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    dependency.dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
    dependency.dstAccessMask = vk::AccessFlagBits::eInputAttachmentRead;
  }
  // depth input attachment can ony be read in frag shader
  else if (dst_layout == vk::ImageLayout::eDepthStencilReadOnlyOptimal) {
    dependency.dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;
    dependency.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead;
  }
  // transfer image should be done until next transfer operation
  else if (dst_layout == vk::ImageLayout::eTransferSrcOptimal) {
    dependency.dstStageMask = vk::PipelineStageFlagBits::eTransfer;
    dependency.dstAccessMask = vk::AccessFlagBits::eTransferRead;
  }
  // color attachment must be ready for next write
  else if (dst_layout == vk::ImageLayout::eColorAttachmentOptimal) {
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
  }
  // depth attachment must be done before early frag test to get valid early z
  else if (dst_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
    dependency.dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
  }
  // presentation after renderpass, taken from
  // https://github.com/GameTechDev/IntroductionToVulkan/blob/master/Project/Tutorial04/Tutorial04.cpp
  else if (dst_layout == vk::ImageLayout::ePresentSrcKHR) {
    dependency.dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
    dependency.dstAccessMask = vk::AccessFlagBits::eMemoryRead;
  }
  else {
    throw std::runtime_error{"destination layout" + to_string(dst_layout) + " not supported"};
  }
  // goal of subpasses is to have no full barriers between
  // define dependencys only per-tile 
  if (src_pass != VK_SUBPASS_EXTERNAL && dst_pass != VK_SUBPASS_EXTERNAL) {
    dependency.dependencyFlags = vk::DependencyFlagBits::eByRegion;
  }

  return dependency;
}

vk::AttachmentDescription img_to_attachment(vk::ImageCreateInfo const& img_info, bool clear) {
  vk::AttachmentDescription attachment{};
  attachment.format = img_info.format;
  // if image is cleared, initial layout doesnt matter as data is deleted 
  if(clear) {
    attachment.loadOp = vk::AttachmentLoadOp::eClear;    
    attachment.stencilLoadOp = vk::AttachmentLoadOp::eClear;
    attachment.initialLayout = vk::ImageLayout::eUndefined;
  }
  else {
    attachment.loadOp = vk::AttachmentLoadOp::eLoad;
    attachment.stencilLoadOp = vk::AttachmentLoadOp::eLoad;
    attachment.initialLayout = img_info.initialLayout;
  }
  // store data only when it is used afterwards
  if ((img_info.usage & vk::ImageUsageFlagBits::eSampled) 
   || (img_info.usage & vk::ImageUsageFlagBits::eStorage)
   || (img_info.usage & vk::ImageUsageFlagBits::eInputAttachment)
   || (img_info.usage & vk::ImageUsageFlagBits::eTransferSrc)
   || img_info.initialLayout == vk::ImageLayout::ePresentSrcKHR) {
    attachment.storeOp = vk::AttachmentStoreOp::eStore;
    attachment.stencilStoreOp = vk::AttachmentStoreOp::eStore;
  }
  else {
    attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  }
  // layout should be preserved, so go back to original after pass
  attachment.finalLayout = img_info.initialLayout;

  return attachment;
}

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
    if (attachments[ref.attachment].initialLayout == vk::ImageLayout::ePresentSrcKHR
     || attachments[ref.attachment].initialLayout == vk::ImageLayout::eTransferDstOptimal
     || attachments[ref.attachment].initialLayout == vk::ImageLayout::eTransferSrcOptimal
        ) {
      dependencies.emplace_back(img_to_dependency(attachments[ref.attachment].initialLayout, ref.layout, VK_SUBPASS_EXTERNAL, 0));
    }
  }
  // add dependency for present image as last stage output 
  for(auto const& ref : sub_passes.back().outputs()) {
    if (attachments[ref.attachment].finalLayout == vk::ImageLayout::ePresentSrcKHR
     || attachments[ref.attachment].finalLayout == vk::ImageLayout::eTransferSrcOptimal) {
      dependencies.emplace_back(img_to_dependency(ref.layout, attachments[ref.attachment].finalLayout, int32_t(sub_passes.size()) - 1, VK_SUBPASS_EXTERNAL));
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