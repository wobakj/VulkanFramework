#ifndef MODEL_LOADER_HPP
#define MODEL_LOADER_HPP

#include "wrap/vertex_data.hpp"
#include "ren/material.hpp"

#include <vector>

namespace vklod {
  class bvh;
}

namespace lamure {
namespace ren {
  class lod_stream;
}
}

namespace model_loader {
std::pair<std::vector<vertex_data>, std::vector<material_t>> objs(std::string const& file_path, vertex_data::attrib_flag_t import_attribs);
vertex_data obj(std::string const& path, vertex_data::attrib_flag_t import_attribs = vertex_data::POSITION);

vertex_data bvh(std::string const& path, std::size_t idx_node);
vklod::bvh bvh(std::string const& path);
lamure::ren::lod_stream&& lod(std::string const& path);
}

#endif