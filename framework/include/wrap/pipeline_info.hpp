#ifndef PIPELINE_INFO_HPP
#define PIPELINE_INFO_HPP

#include <vulkan/vulkan.hpp>
#include <vector>
#include <iostream>

class Shader;

class SpecInfo {
 public:
  SpecInfo()
  {}

  SpecInfo(SpecInfo const& rhs)
   :SpecInfo{}
  {
    for(auto const& constant : rhs.info_spec_entries) {
      setSpecConstant(constant.constantID, constant.size, rhs.buffer_spec_constants.data() + constant.offset);
    }
  }
  // TODO: implement or prevent moving
  // SpecInfo& operator=(SpecInfo info) {
  //   std::swap(*this, info);
  // }

  // SpecInfo(SpecInfo&& rhs) = delete;

  vk::SpecializationInfo const& get() const {
    return info_spec;
  }

  operator vk::SpecializationInfo const&() const {
    return info_spec;
  }

  void setSpecConstant(uint32_t id, size_t size, void const* ptr) {
    bool existing = false;
    for (uint32_t i = 0; i < info_spec_entries.size(); ++i) {
      if (info_spec_entries[i].constantID == id) {
        existing = true;
        assert(size == info_spec_entries[i].size);
        setSpecConstant(i, ptr);
        break;
      }
    }
    if (!existing) {
      addSpecConstant(id, size);
    }
    setSpecConstant(uint32_t(info_spec_entries.size() - 1), ptr);
  }

 private:
  void addSpecConstant(uint32_t id, size_t size) {
    vk::SpecializationMapEntry entry{};
    entry.constantID = id;
    entry.offset = uint32_t(buffer_spec_constants.size());
    entry.size = size;
    info_spec_entries.emplace_back(entry);
    buffer_spec_constants.resize(buffer_spec_constants.size() + size);
    // update reference to buffer
    info_spec.mapEntryCount = uint32_t(info_spec_entries.size());
    info_spec.pMapEntries = info_spec_entries.data();
    info_spec.dataSize = uint32_t(buffer_spec_constants.size());
    info_spec.pData = buffer_spec_constants.data();
  }

  void setSpecConstant(uint32_t id, void const* ptr) {
    std::memcpy(buffer_spec_constants.data() + info_spec_entries[id].offset, ptr, info_spec_entries[id].size);
  }

  std::vector<uint8_t> buffer_spec_constants;
  vk::SpecializationInfo info_spec;
  std::vector<vk::SpecializationMapEntry> info_spec_entries;
};

template<typename T>
class PipelineInfo {
 public:
  PipelineInfo()
   :info{}
  {
    info.flags = vk::PipelineCreateFlagBits::eAllowDerivatives;
    info.basePipelineIndex = -1;
  }

  virtual void setShader(Shader const& shader) = 0;

  void setRoot(vk::Pipeline const& root) {
    info.flags |= vk::PipelineCreateFlagBits::eDerivative;
    // insert previously created pipeline here to derive this one from
    info.basePipelineHandle = root;
  }
  
  operator T const&() const {
    return info;
  }

  vk::PipelineLayout const& layout() const {
    return info.layout;
  }

 protected:

  T info;

};

#endif