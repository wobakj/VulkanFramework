#ifndef MODEL_LOADER_HPP
#define MODEL_LOADER_HPP

#include "model_t.hpp"

#include "tiny_obj_loader.h"

// namespace tinyobj {
// bool operator<(tinyobj::index_t const& lhs,
//                       tinyobj::index_t const& rhs) {
//   if (lhs.vertex_index == rhs.vertex_index) {
//     if (lhs.normal_index == rhs.normal_index) {
//       return lhs.texcoord_index < rhs.texcoord_index;
//     } else
//       return lhs.normal_index < rhs.normal_index;
//   } else
//     return lhs.vertex_index < rhs.vertex_index;
// }
// };

namespace vklod {
  class bvh;
}

namespace lamure {
namespace ren {
  class lod_stream;
}
}

namespace model_loader {

model_t obj(std::string const& path, model_t::attrib_flag_t import_attribs = model_t::POSITION);

model_t bvh(std::string const& path, std::size_t idx_node);
vklod::bvh bvh(std::string const& path);
lamure::ren::lod_stream&& lod(std::string const& path);
}

#endif