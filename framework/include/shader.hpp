#ifndef SHADER_HPP
#define SHADER_HPP

#include "device.hpp"
#include "wrapper.hpp"

#include "spirv_cross.hpp"
#include <vulkan/vulkan.hpp>

#include <map>
#include <vector>

vk::ShaderStageFlagBits execution_to_stage(spv::ExecutionModel const& model);

struct layout_module_t {

  void add_resource(std::string const& name, unsigned set, unsigned binding, unsigned num, vk::DescriptorType const& type) {
    if (bindings.size() <= set) {
      bindings.resize(set + 1);
    }
    vk::DescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = type;
    layoutBinding.descriptorCount = num;
    layoutBinding.stageFlags = stage;
    bindings.at(set).emplace(name, layoutBinding);
  }

  layout_module_t(spirv_cross::Compiler const& comp)
   :stage{execution_to_stage(comp.get_execution_model())}
   {
    // The SPIR-V is now parsed, and we can perform reflection on it.
    spirv_cross::ShaderResources resources = comp.get_shader_resources();
    // Get all sampled images in the shader.
    auto add_func = [&comp, this](spirv_cross::Resource const& resource, vk::DescriptorType const& type) {
      auto const& name = comp.get_name(resource.id);
      unsigned set = comp.get_decoration(resource.id, spv::DecorationDescriptorSet);
      unsigned binding = comp.get_decoration(resource.id, spv::DecorationBinding);
      auto const& res_type = comp.get_type(resource.type_id); 
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
  }
  // check if descriptor is contained
  bool has_descriptor(std::string const& name) {
    for (auto const& set : bindings) {
      if (set.find(name) != set.end()) {
        return true;
      }
    }
    return false;
  }

  std::vector<std::map<std::string, vk::DescriptorSetLayoutBinding>> bindings;
  vk::ShaderStageFlagBits stage;
};

struct layout_shader_t {
  layout_shader_t(){};
  layout_shader_t(std::vector<layout_module_t> const& mod) 
   :modules{mod}
  {
    for(std::size_t i = 0; i < modules.size(); ++i) {
      for(std::size_t idx_set = 0; idx_set < modules[i].bindings.size(); ++idx_set) {
        //resize shader bindings if module has more sets
        if(bindings.size() <= idx_set) {
          bindings.resize(idx_set + 1);
        }
        for(auto const& pair_desc : modules[i].bindings[idx_set]) {
          auto descriptor = pair_desc.second;
          for(std::size_t j = i + 1; j < modules.size(); ++j) {
            auto iter = modules[j].bindings[idx_set].find(pair_desc.first);
            // descriptor with same name exists in set
            if (iter != modules[j].bindings[idx_set].end()) {
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
              for(auto const& pair_desc2 : modules[j].bindings[idx_set]) {
                if (pair_desc2.second.binding == descriptor.binding 
                 && pair_desc2.second.descriptorType == descriptor.descriptorType ) {
                  throw std::runtime_error{"Descriptor '" + pair_desc.first + "' and '" + pair_desc2.first + "' have the same set and binding"};
                }
              }
            }
          }
          bindings[idx_set].emplace(pair_desc.first, descriptor);
        }
      }
    }
  }

  std::vector<std::map<std::string, vk::DescriptorSetLayoutBinding>> bindings;
  std::vector<layout_module_t> modules;
};

std::vector<vk::DescriptorSetLayout> to_set_layouts(vk::Device const& device, layout_shader_t const& shader);

vk::PipelineLayout to_pipe_layout(vk::Device const& device, std::vector<vk::DescriptorSetLayout> const& set_layouts);

using WrapperShader = Wrapper<vk::PipelineLayout, layout_shader_t>;
class Shader : public WrapperShader {
 public:
  Shader();
  Shader(Device const& device, std::vector<std::string> const& paths);
  Shader(Shader && dev);
  Shader(Shader const&) = delete;

  Shader& operator=(Shader const&) = delete;
  Shader& operator=(Shader&& dev);

  void swap(Shader& dev);

  vk::PipelineLayout const& pipelineLayout() const;
  std::vector<vk::PipelineShaderStageCreateInfo> const& shaderStages() const;
  vk::DescriptorSetLayout const& setLayout(std::size_t index) const;
  std::vector<vk::DescriptorSetLayout> const& setLayouts() const;
 private:
  void destroy() override;

  Device const* m_device;
  std::vector<std::string> m_paths;
  std::vector<vk::ShaderModule> m_modules;
  std::vector<vk::DescriptorSetLayout> m_set_layouts;
  std::vector<vk::PipelineShaderStageCreateInfo> m_shader_stages;
};

#endif