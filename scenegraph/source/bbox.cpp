#include "bbox.hpp"

#include "node/node.hpp"
#include "hit.hpp"
#include "ray.hpp"

#include <numeric>

Bbox::Bbox(): 
	m_min(glm::vec3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max())),
	m_max(glm::vec3(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()))
{}

Bbox::Bbox(glm::vec3 const& p):
	m_min(p),
	m_max(p)
{}

Bbox::Bbox(glm::vec3 const& min_p, glm::vec3 const& max_p):
	m_min(min_p),
	m_max(max_p)
{}

glm::vec3 const& Bbox::getMin() const
{
	return m_min;
}

glm::vec3 const& Bbox::getMax() const
{
	return m_max;
}

void Bbox::setMin(glm::vec3 const& min)
{
	m_min = min;
}

void Bbox::setMax(glm::vec3 const& max)
{
	m_max = max;
}

void Bbox::transformBox(glm::mat4 const& transform)
{
	glm::vec4 newPoint = transform * glm::vec4(m_min, 1.0f);
	m_min = glm::vec3(newPoint.x, newPoint.y, newPoint.z);
	
	newPoint = transform * glm::vec4(m_max, 1.0f);
	m_max = glm::vec3(newPoint.x, newPoint.y, newPoint.z);
}

bool Bbox::isEmpty() const
{
	return (m_min.x > m_max.x || m_min.y > m_max.y || m_min.z || m_max.z);
}

std::pair<glm::vec3, glm::vec3> Bbox::corners() const
{
	return std::pair<glm::vec3, glm::vec3>(m_min, m_max);
}

bool Bbox::contains(glm::vec3 &p) const
{
	if (p.x < m_min.x || p.x > m_max.x) return false;
	if (p.y < m_min.y || p.y > m_max.y) return false;
	if (p.z < m_min.z || p.z > m_max.z) return false;

	return true;
}

bool Bbox::intersects(Bbox const &box) const
{
	if (box.m_max.x < m_min.x || box.m_min.x > m_max.x) return false;
	if (box.m_max.y < m_min.y || box.m_min.y > m_max.y) return false;
	if (box.m_max.z < m_min.z || box.m_min.z > m_max.z) return false;

	return true;
}

glm::vec3 Bbox::center() const
{
	return glm::vec3((m_max.x + m_min.z) / 2, (m_max.y + m_min.y) / 2, (m_max.z + m_min.z) / 2);
}

glm::vec3 Bbox::getPosVertex(glm::vec4 &normal) const
{

	glm::vec3 res = m_min;

	if (normal.x > 0)
		res.x = m_max.x;

	if (normal.y > 0)
		res.y = m_max.y;

	if (normal.z > 0)
		res.z = m_max.z;

	return res;
}

glm::vec3 Bbox::getNegVertex(glm::vec4 &normal) const
{
	glm::vec3 res = m_min;

	if (normal.x < 0)
		res.x = m_max.x;

	if (normal.y < 0)
		res.y = m_max.y;

	if (normal.z < 0)
		res.z = m_max.z;

	return res;
}

Hit Bbox::intersect(Ray const& r, glm::fmat4 const& mat_world) {
	float tMin = 0.0f;
	float tMax = 100000.0f;

	Hit hit{};
	glm::vec3 obox_worldpos(mat_world[3][0], mat_world[3][1], mat_world[3][2]);
	glm::vec4 invPoint = glm::vec4(getMin(), 1.0f) * glm::inverse(mat_world);
	glm::vec3 aabb_min = glm::vec3(invPoint.x, invPoint.y, invPoint.z);
	invPoint = glm::vec4(getMax(), 1.0f) * glm::inverse(mat_world);
	glm::vec3 aabb_max = glm::vec3(invPoint.x, invPoint.y, invPoint.z);

	glm::vec3 delta = obox_worldpos - glm::vec3(r.getOrigin().x, r.getOrigin().y, r.getOrigin().z);

	// Test intersection with the 2 planes perpendicular to the OBB's X axis
	{

		glm::vec3 x_axis(mat_world[0][0], mat_world[0][1], mat_world[0][2]);
		float e = glm::dot(x_axis, delta);
		float f = glm::dot(glm::vec3(r.getDir().x, r.getDir().y, r.getDir().z), x_axis);

		if (std::fabs(f) > 0.001f)
		{

			float t1 = (e + aabb_min.x) / f; // Intersection with the left plane
			float t2 = (e + aabb_max.x)/f; // Intersection with the right plane

			if (t1 > t2){
				float w = t1; t1 = t2;t2 = w; // swap t1 and t2
			}

			// tMax is the nearest "far" intersection (amongst the X,Y and Z planes pairs)
			if ( t2 < tMax )
				tMax = t2;
			// tMin is the farthest "near" intersection (amongst the X,Y and Z planes pairs)
			if ( t1 > tMin )
				tMin = t1;

			// And here's the trick :
			// If "far" is closer than "near", then there is NO intersection.
			// See the images in the tutorials for the visual explanation.
			if (tMax < tMin) 
			{
				return hit;
			}

		}
		else
		{ // Rare case : the ray is almost parallel to the planes, so they don't have any "intersection"
			if(-e + aabb_min.x > 0.0f || -e + aabb_max.x < 0.0f)
			{
				return hit;
			}
		}
	}

	{
		glm::vec3 y_axis(mat_world[1][0], mat_world[1][1], mat_world[1][2]);
		float e = glm::dot(y_axis, delta);
		float f = glm::dot(glm::vec3(r.getDir().x, r.getDir().y, r.getDir().z), y_axis);

		if (std::fabs(f) > 0.001f)
		{

			float t1 = (e + aabb_min.y) / f;
			float t2 = (e + aabb_max.y) / f;

			if (t1 > t2)
			{
				float w = t1; t1 = t2; t2 = w;
			}

			if ( t2 < tMax )
				tMax = t2;
			if ( t1 > tMin )
				tMin = t1;
			if (tMin > tMax)
			{
				return hit;
			}

		}
		else
		{
			if(-e + aabb_min.y > 0.0f || -e + aabb_max.y < 0.0f)
			{
				return hit;
			}
		}
	}

	{
		glm::vec3 z_axis(mat_world[2][0], mat_world[2][1], mat_world[2][2]);
		float e = glm::dot(z_axis, delta);
		float f = glm::dot(glm::vec3(r.getDir().x, r.getDir().y, r.getDir().z), z_axis);

		if (std::fabs(f) > 0.001f)
		{
			float t1 = (e + aabb_min.z) / f;
			float t2 = (e + aabb_max.z) / f;

			if (t1 > t2)
			{
				float w = t1; t1 = t2; t2 = w;
			}

			if ( t2 < tMax )
				tMax = t2;
			if ( t1 > tMin )
				tMin = t1;
			if (tMin > tMax)
			{
				return hit;
			}

		}
		else
		{
			if(-e + aabb_min.z > 0.0f || -e+aabb_max.z < 0.0f)
			{
				return hit;
			}
		}
	}
	hit.success();
	hit.setDistToHit(tMin);
	return hit;
}