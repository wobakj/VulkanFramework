#include "model_t.hpp"

#include <cstdint>

std::vector<model_t::attribute> const model_t::VERTEX_ATTRIBS
 = {  
    /*POSITION*/{ 1 << 0, std::uint32_t(sizeof(float) * 3), vk::Format::eR32G32B32Sfloat},
    /*NORMAL*/{   1 << 1, std::uint32_t(sizeof(float) * 3), vk::Format::eR32G32B32Sfloat},
    /*TEXCOORD*/{ 1 << 2, std::uint32_t(sizeof(float) * 2), vk::Format::eR32G32Sfloat},
    /*TANGENT*/{  1 << 3, std::uint32_t(sizeof(float) * 3), vk::Format::eR32G32B32Sfloat},
    /*BITANGENT*/{1 << 4, std::uint32_t(sizeof(float) * 3), vk::Format::eR32G32B32Sfloat}
 };

model_t::attribute const& model_t::POSITION = model_t::VERTEX_ATTRIBS[0];
model_t::attribute const& model_t::NORMAL = model_t::VERTEX_ATTRIBS[1];
model_t::attribute const& model_t::TEXCOORD = model_t::VERTEX_ATTRIBS[2];
model_t::attribute const& model_t::TANGENT = model_t::VERTEX_ATTRIBS[3];
model_t::attribute const& model_t::BITANGENT = model_t::VERTEX_ATTRIBS[4];
model_t::attribute const  model_t::INDEX{1 << 5, std::uint32_t(sizeof(unsigned)), vk::Format::eR32Uint};

model_t::model_t()
 :data{}
 ,indices{}
 ,offsets{}
 ,vertex_bytes{0}
 ,vertex_num{0}
{}

model_t::model_t(std::vector<float> const& databuff, attrib_flag_t contained_attributes, std::vector<std::uint32_t> const& trianglebuff)
 :data(databuff)
 ,indices(trianglebuff)
 ,offsets{}
 ,vertex_bytes{0}
 ,vertex_num{0}
{
  for (auto const& supported_attribute : model_t::VERTEX_ATTRIBS) {
    // check if buffer contains attribute
    if (supported_attribute.flag & contained_attributes) {
      // write offset, explicit cast to prevent narrowing warning
      offsets.insert(std::pair<attrib_flag_t, std::uint32_t>{supported_attribute, vertex_bytes});
      // move offset pointer forward
      vertex_bytes += supported_attribute.size;
    }
  }
  // set number of vertice sin buffer
  vertex_num = std::uint32_t(data.size() * sizeof(float) / vertex_bytes);
}