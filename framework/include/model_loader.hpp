#ifndef MODEL_LOADER_HPP
#define MODEL_LOADER_HPP

#include "model_t.hpp"

#include "tiny_obj_loader.h"

namespace model_loader {

model_t obj(std::string const& path, model_t::attrib_flag_t import_attribs = model_t::POSITION);

model_t bvh(std::string const& path, std::size_t idx_node  = 0);
}

#endif