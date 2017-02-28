#include "wrap/vertex_data.hpp"

#include <cstdint>

std::vector<vertex_data::attribute> const vertex_data::VERTEX_ATTRIBS
 = {  
    /*POSITION*/{ 1 << 0, std::uint32_t(sizeof(float) * 3), vk::Format::eR32G32B32Sfloat},
    /*NORMAL*/{   1 << 1, std::uint32_t(sizeof(float) * 3), vk::Format::eR32G32B32Sfloat},
    /*TEXCOORD*/{ 1 << 2, std::uint32_t(sizeof(float) * 2), vk::Format::eR32G32Sfloat},
    /*TANGENT*/{  1 << 3, std::uint32_t(sizeof(float) * 3), vk::Format::eR32G32B32Sfloat},
    /*BITANGENT*/{1 << 4, std::uint32_t(sizeof(float) * 3), vk::Format::eR32G32B32Sfloat}
 };

vertex_data::attribute const& vertex_data::POSITION = vertex_data::VERTEX_ATTRIBS[0];
vertex_data::attribute const& vertex_data::NORMAL = vertex_data::VERTEX_ATTRIBS[1];
vertex_data::attribute const& vertex_data::TEXCOORD = vertex_data::VERTEX_ATTRIBS[2];
vertex_data::attribute const& vertex_data::TANGENT = vertex_data::VERTEX_ATTRIBS[3];
vertex_data::attribute const& vertex_data::BITANGENT = vertex_data::VERTEX_ATTRIBS[4];
vertex_data::attribute const  vertex_data::INDEX{1 << 5, std::uint32_t(sizeof(unsigned)), vk::Format::eR32Uint};

vertex_data::vertex_data()
 :data{}
 ,indices{}
 ,offsets{}
 ,vertex_bytes{0}
 ,vertex_num{0}
{}

vertex_data::vertex_data(std::vector<float> const& databuff, attrib_flag_t contained_attributes, std::vector<std::uint32_t> const& trianglebuff)
 :data(databuff)
 ,indices(trianglebuff)
 ,offsets{}
 ,vertex_bytes{0}
 ,vertex_num{0}
{
  for (auto const& supported_attribute : vertex_data::VERTEX_ATTRIBS) {
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