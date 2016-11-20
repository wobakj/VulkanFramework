// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#ifndef VKLOD_TYPES_H_
#define VKLOD_TYPES_H_

#include <string>
#include <stdint.h>
#include <assert.h>
#include <limits>
#include "vector3.h"

typedef float float32_t;
typedef double float64_t;

namespace vklod {

// default type for storing coordinates
using real_t = double; //< for surfel position and radius


// math
using vec3f_t  = vklod::vector_t<float32_t, 3>;
using vec3d_t  = vklod::vector_t<float64_t, 3>;
using vec3r_t  = vklod::vector_t<real_t, 3>;
using vec3ui_t = vklod::vector_t<uint32_t, 3>;
using vec3b_t  = vklod::vector_t<uint8_t, 3>;

//rendering system types
using node_t    = size_t;
using slot_t    = size_t;
using model_t   = uint32_t;
using view_t    = uint32_t;
using context_t = uint32_t;

const static node_t invalid_node_t       = std::numeric_limits<node_t>::max();
const static slot_t invalid_slot_t       = std::numeric_limits<slot_t>::max();
const static model_t invalid_model_t     = std::numeric_limits<model_t>::max();
const static view_t invalid_view_t       = std::numeric_limits<view_t>::max();
const static context_t invalid_context_t = std::numeric_limits<context_t>::max();

} // namespace vklod

#endif // VKLOD_TYPES_H_

