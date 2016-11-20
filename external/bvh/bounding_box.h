// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#ifndef VKLOD_BOUNDING_BOX_H_
#define VKLOD_BOUNDING_BOX_H_

#include "types.h"

namespace vklod {

class bounding_box_t
{
public:

    explicit            bounding_box_t() : min_(vec3r_t(1.0)), max_(vec3r_t(-1.0)) {}

    explicit            bounding_box_t(const vec3r_t& min,
                                    const vec3r_t& max)
                            : min_(min), max_(max) {}

    const vec3r_t         min() const { return min_; }
    vec3r_t&              min() { return min_; }

    const vec3r_t         max() const { return max_; }
    vec3r_t&              max() { return max_; }

private:

    vec3r_t               min_;
    vec3r_t               max_;

};

} // namespace vklod

#endif // VKLOD_BOUNDING_BOX_H_

