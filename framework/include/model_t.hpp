#ifndef MODEL_T_HPP
#define MODEL_T_HPP

#include <vulkan/vulkan.hpp>

#include <map>
#include <vector>

// holds vertex information and triangle indices
struct model_t {

  //flag type to combine attributes
  typedef int attrib_flag_t;
  // type holding info about a vertex/model attribute
  struct attribute {

    attribute(attrib_flag_t f, std::uint32_t s, vk::Format t)
     :flag{f}
     ,size{s}
     ,type{t}
    {}

    // conversion to flag type for use as enum
    operator attrib_flag_t const&() const{
      return flag;
    }

    // ugly enum to use as flag, must be unique power of two
    attrib_flag_t flag;
    // size in bytes
    std::uint32_t size;
    // Gl type
    vk::Format type;
    // offset from element beginning
    void* offset;
  };

  // holds all possible vertex attributes, for iteration
  static std::vector<attribute> const VERTEX_ATTRIBS;
  // symbolic values to access valuesin vector by name
  static attribute const& POSITION;
  static attribute const& NORMAL;
  static attribute const& TEXCOORD;
  static attribute const& TANGENT;
  static attribute const& BITANGENT;
  // is not a vertex attribute, so not stored in VERTEX_ATTRIBS
  static attribute const  INDEX;
  
  model_t();
  model_t(std::vector<float> const& databuff, attrib_flag_t attribs, std::vector<std::uint32_t> const& trianglebuff = std::vector<std::uint32_t>{});

  std::vector<float> data;
  std::vector<std::uint32_t> indices;
  // byte offsets of individual element attributes
  std::map<attrib_flag_t, std::uint32_t> offsets;
  // size of one vertex element in bytes
  std::uint32_t vertex_bytes;
  std::uint32_t vertex_num;
};

#endif