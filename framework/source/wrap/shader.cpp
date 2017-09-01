#include "wrap/shader.hpp"

#include "wrap/device.hpp"
#include "wrap/descriptor_set_layout_info.hpp"
#include "shader_loader.hpp"

#include <spirv_cross.hpp>

#include <map>
#include <iostream>

vk::ShaderStageFlagBits execution_to_stage(spv::ExecutionModel const& model) {
  if (model == spv::ExecutionModelVertex) {
    return vk::ShaderStageFlagBits::eVertex;
  }
  else if (model == spv::ExecutionModelTessellationControl) {
    return vk::ShaderStageFlagBits::eTessellationControl;
  }
  else if (model == spv::ExecutionModelTessellationEvaluation) {
    return vk::ShaderStageFlagBits::eTessellationEvaluation;
  }
  else if (model == spv::ExecutionModelGeometry) {
    return vk::ShaderStageFlagBits::eGeometry;
  }
  else if (model == spv::ExecutionModelFragment) {
    return vk::ShaderStageFlagBits::eFragment;
  }
  else if (model == spv::ExecutionModelGLCompute) {
    return vk::ShaderStageFlagBits::eCompute;
  }
  else {
    throw std::runtime_error{"execution model not supported"};
    return vk::ShaderStageFlagBits::eAll;
  }
}

void layout_module_t::add_resource(std::string const& name, unsigned set, unsigned binding, unsigned num, vk::DescriptorType const& type) {
  if (sets.size() <= set) {
    sets.resize(set + 1);
  }
  vk::DescriptorSetLayoutBinding layoutBinding{};
  layoutBinding.binding = binding;
  layoutBinding.descriptorType = type;
  layoutBinding.descriptorCount = num;
  layoutBinding.stageFlags = stage;
  sets.at(set).emplace(name, layoutBinding);
}

layout_module_t::layout_module_t(spirv_cross::Compiler const& comp)
 :stage{execution_to_stage(comp.get_execution_model())}
 {
  // The SPIR-V is now parsed, and we can perform reflection on it.
  spirv_cross::ShaderResources resources = comp.get_shader_resources();
  // generic function to add resource
  auto add_func = [&comp, this](spirv_cross::Resource const& resource, vk::DescriptorType const& type) {
    std::string name{};
    // get the type name from buffers, they dont require instance names
    if (type == vk::DescriptorType::eStorageBuffer
     || type  == vk::DescriptorType::eUniformBuffer) {
      name = comp.get_name(resource.base_type_id);
    }
    else {
      name = comp.get_name(resource.id);
    }
    unsigned set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
    unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
    auto const& res_type = comp.get_type(resource.type_id); 
    if (name == "") {
      throw std::runtime_error{"Descriptor in set " + std::to_string(set) + ", binding " + std::to_string(binding) + " has no name"};
    }
    unsigned num = 1;
    for (auto const& dim : res_type.array) {
      num *= dim;
    }
    add_resource(name, set, binding, num, type);
  };

  for (auto const& resource : resources.sampled_images) {
    add_func(resource, vk::DescriptorType::eCombinedImageSampler);
  }

  for (auto const& resource : resources.uniform_buffers) {
    add_func(resource, vk::DescriptorType::eUniformBuffer);
  }

  for (auto const& resource : resources.storage_buffers) {
    add_func(resource, vk::DescriptorType::eStorageBuffer);
  }
  for (auto const& resource : resources.subpass_inputs) {
    add_func(resource, vk::DescriptorType::eInputAttachment);
  }
  for (auto const& resource : resources.storage_images) {
    add_func(resource, vk::DescriptorType::eStorageImage);
  }
  for (auto const& resource : resources.separate_images) {
    add_func(resource, vk::DescriptorType::eSampledImage);
  }
  for (auto const& resource : resources.separate_samplers) {
    add_func(resource, vk::DescriptorType::eSampler);
  }
  // spec allows only one push constant block per stage
  if (!resources.push_constant_buffers.empty()) {
    auto const& constants = comp.get_active_buffer_ranges(resources.push_constant_buffers.front().id);
    // push constant block variables may be unused -> empty active range
    if (!constants.empty()) {
      // spirv stores for each block member individual range
      // ranges are ordered by offset => compute total offset and size
      push_constant.offset = uint32_t(constants.front().offset);
      push_constant.size = uint32_t(constants.back().offset + constants.back().range - constants.front().offset);
      push_constant.stageFlags = stage;
    }
  }
  // TODO: implement support for dynamic buffer offsets and texel buffers

}
// check if descriptor is contained
bool layout_module_t::has_descriptor(std::string const& name) {
  for (auto const& set : sets) {
    if (set.find(name) != set.end()) {
      return true;
    }
  }
  return false;
}

layout_shader_t::layout_shader_t(std::vector<layout_module_t> const& mod) 
 :modules{mod}
{
  for(std::size_t i = 0; i < modules.size(); ++i) {
    for(std::size_t idx_set = 0; idx_set < modules[i].sets.size(); ++idx_set) {
      //resize shader sets if module has more sets
      if(sets.size() <= idx_set) {
        sets.resize(idx_set + 1);
      }
      for(auto const& pair_desc : modules[i].sets[idx_set]) {
        auto descriptor = pair_desc.second;
        for(std::size_t j = i + 1; j < modules.size(); ++j) {
          if (idx_set >= modules[j].sets.size()) {
            continue;
          }
          auto iter = modules[j].sets[idx_set].find(pair_desc.first);
          // descriptor with same name exists in set
          if (iter != modules[j].sets[idx_set].end()) {
            if (iter->second.binding == descriptor.binding) {
              if (iter->second.descriptorType == descriptor.descriptorType) {
                if (iter->second.descriptorCount == descriptor.descriptorCount) {
                  descriptor.stageFlags |= modules[j].stage;
                }
                // check if count matches
                else {
                  throw std::runtime_error{"Descriptor '" + pair_desc.first + "' count varies between stages"};
                }
              }
              // check if type matches
              else {
                throw std::runtime_error{"Descriptor '" + pair_desc.first + "' type varies between stages"};
              }
            }
            // make sure desriptor with same name has same binding
            else {
              throw std::runtime_error{"Descriptor '" + pair_desc.first + "' binding varies between stages"};
            }
          }
          else {
            // make sure no descriptor of same name exists in different set
            if (modules[j].has_descriptor(pair_desc.first)) {
              throw std::runtime_error{"Descriptor '" + pair_desc.first + "' set varies between stages"};
            }
            // make sure no other descriptor is bound to same binding 
            for(auto const& pair_desc2 : modules[j].sets[idx_set]) {
              if (pair_desc2.second.binding == descriptor.binding 
               && pair_desc2.second.descriptorType == descriptor.descriptorType ) {
                throw std::runtime_error{"Descriptor '" + pair_desc.first + "' and '" + pair_desc2.first + "' have the same set and binding"};
              }
            }
          }
        }
        sets[idx_set].emplace(pair_desc.first, descriptor);
      }
    }
    // module contains push constant -> add to list
    if(modules[i].push_constant.size > 0) {
      // check if shader already contains identical push constant -> merge
      bool identical = false;
      for (auto& range : push_constants) {
        if (range.offset == modules[i].push_constant.offset
          && range.size == modules[i].push_constant.size) {
          range.stageFlags |= modules[i].push_constant.stageFlags;
          identical = true; 
          std::cout << "identical" << std::endl;
          break;
        }
      }
      // no identical range -> add new one
      if (!identical) {
        push_constants.emplace_back(modules[i].push_constant);
      }
    }
  }
}

DescriptorSetLayout to_set_layout(vk::Device const& device, std::map<std::string, vk::DescriptorSetLayoutBinding> const& set) {
  DescriptorSetLayoutInfo layout_info{};
  for(auto const& pair_desc : set) {
    layout_info.setBinding(pair_desc.second);
  }
  return DescriptorSetLayout{device, layout_info};
}

std::vector<DescriptorSetLayout> to_set_layouts(vk::Device const& device, layout_shader_t const& shader) {
  std::vector<DescriptorSetLayout> set_layouts{};
  for(auto const& set : shader.sets) {
    set_layouts.emplace_back(to_set_layout(device, set));
  }
  return set_layouts;
}

vk::PipelineLayout to_pipe_layout(vk::Device const& device, std::vector<DescriptorSetLayout> const& set_layouts, std::vector<vk::PushConstantRange> const& push_constants) {
  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
  std::vector<vk::DescriptorSetLayout> vklayouts{};
  for (auto const& layout : set_layouts) {
    vklayouts.emplace_back(layout.get());
  }
  pipelineLayoutInfo.setLayoutCount = std::uint32_t(vklayouts.size());
  pipelineLayoutInfo.pSetLayouts = vklayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = std::uint32_t(push_constants.size());
  pipelineLayoutInfo.pPushConstantRanges = push_constants.data();

  return device.createPipelineLayout(pipelineLayoutInfo);
}

Shader::Shader()
{}

Shader::Shader(Device const& device, std::vector<std::string> const& paths)
 :m_device{device}
 ,m_paths{paths}
 {
  std::vector<layout_module_t> module_layouts{};
  for(auto const& path : m_paths) {
    auto code = shader_loader::read_file(path);
    m_modules.emplace_back(shader_loader::module(code, device));

    module_layouts.emplace_back(shader_loader::layout(code));

    vk::PipelineShaderStageCreateInfo stage_info{};
    stage_info.stage = module_layouts.back().stage;
    stage_info.module = m_modules.back();
    stage_info.pName = "main";
    m_shader_stages.emplace_back(stage_info);
  }

  m_info = layout_shader_t{module_layouts};
  m_set_layouts = to_set_layouts(device, info());
  m_object = to_pipe_layout(device, m_set_layouts, m_info.push_constants);
 }

void Shader::destroy() {
  m_device.destroyPipelineLayout(get());
  for(auto const& module : m_modules) {
    m_device.destroyShaderModule(module);
  }
}

Shader::Shader(Shader && dev)
 :Shader{}
{
  swap(dev);
}

Shader::~Shader() {
  cleanup();
}

Shader& Shader::operator=(Shader&& dev) {
  swap(dev);
  return *this;
}

void Shader::swap(Shader& dev) {
  WrapperShader::swap(dev);
  std::swap(m_device, dev.m_device);
  std::swap(m_paths, dev.m_paths);
  std::swap(m_modules, dev.m_modules);
  std::swap(m_set_layouts, dev.m_set_layouts);
  std::swap(m_shader_stages, dev.m_shader_stages);
}

vk::PipelineLayout const& Shader::pipelineLayout() const {
  return get();
}

DescriptorSetLayout const& Shader::setLayout(std::size_t index) const {
  return m_set_layouts[index];
}

std::vector<DescriptorSetLayout> const& Shader::setLayouts() const {
  return m_set_layouts;
}

std::vector<vk::PipelineShaderStageCreateInfo> const& Shader::shaderStages() const {
  return m_shader_stages;
}

vk::GraphicsPipelineCreateInfo Shader::startPipelineInfo() const {
  vk::GraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.stageCount = uint32_t(shaderStages().size());
  pipelineInfo.pStages = shaderStages().data();
  pipelineInfo.layout = pipelineLayout();

  return pipelineInfo;
}

std::vector<std::string> const& Shader::paths() const {
  return m_paths;
}
