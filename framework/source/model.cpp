#include "model.hpp"

#include <cstdint>

std::vector<model::attribute> const model::VERTEX_ATTRIBS
 = {  
    /*POSITION*/{ 1 << 0, std::uint32_t(sizeof(float) * 3), vk::Format::eR32G32B32Sfloat},
    /*NORMAL*/{   1 << 1, std::uint32_t(sizeof(float) * 3), vk::Format::eR32G32B32Sfloat},
    /*TEXCOORD*/{ 1 << 2, std::uint32_t(sizeof(float) * 2), vk::Format::eR32G32Sfloat},
    /*TANGENT*/{  1 << 3, std::uint32_t(sizeof(float) * 3), vk::Format::eR32G32B32Sfloat},
    /*BITANGENT*/{1 << 4, std::uint32_t(sizeof(float) * 3), vk::Format::eR32G32B32Sfloat}
 };

model::attribute const& model::POSITION = model::VERTEX_ATTRIBS[0];
model::attribute const& model::NORMAL = model::VERTEX_ATTRIBS[1];
model::attribute const& model::TEXCOORD = model::VERTEX_ATTRIBS[2];
model::attribute const& model::TANGENT = model::VERTEX_ATTRIBS[3];
model::attribute const& model::BITANGENT = model::VERTEX_ATTRIBS[4];
model::attribute const  model::INDEX{1 << 5, std::uint32_t(sizeof(unsigned)), vk::Format::eR32Uint};

model::model()
 :data{}
 ,indices{}
 ,offsets{}
 ,vertex_bytes{0}
 ,vertex_num{0}
{}

model::model(std::vector<float> const& databuff, attrib_flag_t contained_attributes, std::vector<std::uint32_t> const& trianglebuff)
 :data(databuff)
 ,indices(trianglebuff)
 ,offsets{}
 ,vertex_bytes{0}
 ,vertex_num{0}
{
  for (auto const& supported_attribute : model::VERTEX_ATTRIBS) {
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