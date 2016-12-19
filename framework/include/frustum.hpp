#ifndef FRUSTUM_HPP
#define FRUSTUM_HPP
/*
* View frustum culling class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/
#include "bounding_box.h"

#include <array>
#include <math.h>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtx/intersect.hpp>

class Frustum
{
 public:
	void update(glm::fmat4 const& matrix)
	{
		planes[LEFT].x = matrix[0].w + matrix[0].x;
		planes[LEFT].y = matrix[1].w + matrix[1].x;
		planes[LEFT].z = matrix[2].w + matrix[2].x;
		planes[LEFT].w = matrix[3].w + matrix[3].x;

		planes[RIGHT].x = matrix[0].w - matrix[0].x;
		planes[RIGHT].y = matrix[1].w - matrix[1].x;
		planes[RIGHT].z = matrix[2].w - matrix[2].x;
		planes[RIGHT].w = matrix[3].w - matrix[3].x;

		planes[TOP].x = matrix[0].w - matrix[0].y;
		planes[TOP].y = matrix[1].w - matrix[1].y;
		planes[TOP].z = matrix[2].w - matrix[2].y;
		planes[TOP].w = matrix[3].w - matrix[3].y;

		planes[BOTTOM].x = matrix[0].w + matrix[0].y;
		planes[BOTTOM].y = matrix[1].w + matrix[1].y;
		planes[BOTTOM].z = matrix[2].w + matrix[2].y;
		planes[BOTTOM].w = matrix[3].w + matrix[3].y;

		planes[BACK].x = matrix[0].w + matrix[0].z;
		planes[BACK].y = matrix[1].w + matrix[1].z;
		planes[BACK].z = matrix[2].w + matrix[2].z;
		planes[BACK].w = matrix[3].w + matrix[3].z;

		planes[FRONT].x = matrix[0].w - matrix[0].z;
		planes[FRONT].y = matrix[1].w - matrix[1].z;
		planes[FRONT].z = matrix[2].w - matrix[2].z;
		planes[FRONT].w = matrix[3].w - matrix[3].z;

		for (std::size_t i = 0; i < planes.size(); i++) {
			float length = sqrtf(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
			planes[i] /= length;
		}
		// calculate corner points
		auto inverted = glm::inverse(matrix);
		points[0] = glm::fvec3{inverted * glm::fvec4{1.0f, 1.0f, 0.0f, 1.0f}};
		points[1] = glm::fvec3{inverted * glm::fvec4{1.0f, 1.0f, 1.0f, 1.0f}};
		points[2] = glm::fvec3{inverted * glm::fvec4{-1.0f, 1.0f, 0.0f, 1.0f}};
		points[3] = glm::fvec3{inverted * glm::fvec4{-1.0f, 1.0f, 1.0f, 1.0f}};
		points[4] = glm::fvec3{inverted * glm::fvec4{1.0f, -1.0f, 0.0f, 1.0f}};
		points[5] = glm::fvec3{inverted * glm::fvec4{1.0f, -1.0f, 1.0f, 1.0f}};
		points[6] = glm::fvec3{inverted * glm::fvec4{-1.0f, -1.0f, 0.0f, 1.0f}};
		points[7] = glm::fvec3{inverted * glm::fvec4{-1.0f, -1.0f, 1.0f, 1.0f}};
	}
	
	bool checkSphere(glm::vec3 pos, float radius) const	{
		for (std::size_t i = 0; i < planes.size(); i++) {
			if ((planes[i].x * pos.x) + (planes[i].y * pos.y) + (planes[i].z * pos.z) + planes[i].w <= -radius)	{
				return false;
			}
		}
		return true;
	}
// from http://www.iquilezles.org/www/articles/frustumcorrect/frustumcorrect.htm
	bool intersects(vklod::bounding_box_t const& box) const {
	  // check box outside/inside of frustum
    for (int i = 0; i < 6; ++i) {
      int out = 0;
      out += ((glm::dot(planes[i], glm::fvec4(box.min()[0], box.min()[1], box.min()[2], 1.0f)) < 0.0 ) ? 1 : 0);
      out += ((glm::dot(planes[i], glm::fvec4(box.max()[0], box.min()[1], box.min()[2], 1.0f)) < 0.0 ) ? 1 : 0);
      out += ((glm::dot(planes[i], glm::fvec4(box.min()[0], box.max()[1], box.min()[2], 1.0f)) < 0.0 ) ? 1 : 0);
      out += ((glm::dot(planes[i], glm::fvec4(box.max()[0], box.max()[1], box.min()[2], 1.0f)) < 0.0 ) ? 1 : 0);
      out += ((glm::dot(planes[i], glm::fvec4(box.min()[0], box.min()[1], box.max()[2], 1.0f)) < 0.0 ) ? 1 : 0);
      out += ((glm::dot(planes[i], glm::fvec4(box.max()[0], box.min()[1], box.max()[2], 1.0f)) < 0.0 ) ? 1 : 0);
      out += ((glm::dot(planes[i], glm::fvec4(box.min()[0], box.max()[1], box.max()[2], 1.0f)) < 0.0 ) ? 1 : 0);
      out += ((glm::dot(planes[i], glm::fvec4(box.max()[0], box.max()[1], box.max()[2], 1.0f)) < 0.0 ) ? 1 : 0);
      if (out == 8) return false;
    }

    // check frustum outside/inside box
    int out;
    out = 0; for (int i = 0; i < 8; ++i) out += ((points[i].x > box.max()[0]) ? 1 : 0); 
    if (out == 8) return false;
    out = 0; for (int i = 0; i < 8; ++i) out += ((points[i].x < box.min()[0]) ? 1 : 0); 
    if (out == 8) return false;
    out = 0; for (int i = 0; i < 8; ++i) out += ((points[i].y > box.max()[1]) ? 1 : 0); 
    if (out == 8) return false;
    out = 0; for (int i = 0; i < 8; ++i) out += ((points[i].y < box.min()[1]) ? 1 : 0); 
    if (out == 8) return false;
    out = 0; for (int i = 0; i < 8; ++i) out += ((points[i].z > box.max()[2]) ? 1 : 0); 
    if (out == 8) return false;
    out = 0; for (int i = 0; i < 8; ++i) out += ((points[i].z < box.min()[2]) ? 1 : 0); 
    if (out == 8) return false;

    return true;
	}

 private:
	enum side { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, BACK = 4, FRONT = 5 };
	std::array<glm::fvec4, 6> planes;
	std::array<glm::fvec3, 8> points;
};

#endif