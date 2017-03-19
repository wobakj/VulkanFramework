#include "bbox.hpp"

#include "node/node.hpp"
#include "hit.hpp"
#include "ray.hpp"
#include <glm/gtc/type_precision.hpp>

#include <numeric>
#include <iostream>

Bbox::Bbox(): 
	m_min(glm::vec3(std::numeric_limits<float>::max())),
	m_max(glm::vec3(std::numeric_limits<float>::lowest()))
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

void Bbox::transform(glm::mat4 const& transform)
{
	std::vector<glm::vec4> points(8);
	points[0] = glm::vec4(m_min, 1.0f);
	points[1] = glm::vec4(m_max.x, m_min.y, m_min.z, 1.0f);
	points[2] = glm::vec4(m_max.x, m_max.y, m_min.z, 1.0f);
	points[3] = glm::vec4(m_min.x, m_max.y, m_min.z, 1.0f);

	points[4] = glm::vec4(m_min.x, m_max.y, m_max.z, 1.0f);
	points[5] = glm::vec4(m_max, 1.0f);
	points[6] = glm::vec4(m_max.x, m_min.y, m_max.z, 1.0f);
	points[7] = glm::vec4(m_min.x, m_min.y, m_max.z, 1.0f);

	m_min = glm::vec3(std::numeric_limits<float>::max());
	m_max = glm::vec3(std::numeric_limits<float>::lowest());
	for (auto& point : points) {
		point = transform * point;
		if (point.x < m_min.x) m_min.x = point.x;
		if (point.y < m_min.y) m_min.y = point.y;
		if (point.z < m_min.z) m_min.z = point.z;

		if (point.x > m_max.x) m_max.x = point.x;
		if (point.y > m_max.y) m_max.y = point.y;
		if (point.z > m_max.z) m_max.z = point.z;
	}
}

void Bbox::join(Bbox const& b)
{
	if (b.getMin().x < m_min.x) m_min.x = b.getMin().x;
	if (b.getMin().y < m_min.y) m_min.y = b.getMin().y;
	if (b.getMin().z < m_min.z) m_min.z = b.getMin().z;

	if (b.getMax().x > m_max.x) m_max.x = b.getMax().x;
	if (b.getMax().y > m_max.y) m_max.y = b.getMax().y;
	if (b.getMax().z > m_max.z) m_max.z = b.getMax().z;
}

bool Bbox::isEmpty() const
{
	return (m_min.x > m_max.x || m_min.y > m_max.y || m_min.z || m_max.z);
}

std::pair<glm::vec3, glm::vec3> Bbox::corners() const
{
	return std::pair<glm::vec3, glm::vec3>(m_min, m_max);
}

bool Bbox::contains(glm::vec3 const& p) const
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

Hit Bbox::intersects(Ray const& r) {
	Hit h{};

	glm::vec3 normals[6] = { glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0), 
		glm::vec3(0, 0, -1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), 
		glm::vec3(0, 0, 1) };
	float txmin, txmax, tymin, tymax, tzmin, tzmax;

	//check x axis
	if (r.invDir().x >= 0.0f)
	{
		txmin = (m_min.x - r.origin().x) * r.invDir().x;
		txmax = (m_max.x - r.origin().x) * r.invDir().x;
	}
	else
	{
		txmin = (m_max.x - r.origin().x) * r.invDir().x;
		txmax = (m_min.x - r.origin().x) * r.invDir().x;
	}

	// check y axis
	if (r.invDir().y >= 0.0f)
	{
		tymin = (m_min.y - r.origin().y) * r.invDir().y;
		tymax = (m_max.y - r.origin().y) * r.invDir().y;
	}
	else
	{
		tymin = (m_max.y - r.origin().y) * r.invDir().y;
		tymax = (m_min.y - r.origin().y) * r.invDir().y;
	}

	if (txmin > tymax || tymin > txmax)
	{
		h.setSuccess(false);
		return h;
	}

	// check z axis
	if (r.invDir().z >= 0.0f)
	{
		tzmin = (m_min.z - r.origin().z) * r.invDir().z;
		tzmax = (m_max.z - r.origin().z) * r.invDir().z;
	}
	else
	{
		tzmin = (m_max.z - r.origin().z) * r.invDir().z;
		tzmax = (m_min.z - r.origin().z) * r.invDir().z;
	}

	if (txmin > tzmax || tzmin > txmax) 
	{
		h.setSuccess(false);
		return h;
	}

	float t0, t1;
	int normal_in, normal_out;

	if (txmin > tymin)
	{
		t0 = txmin;
		normal_in= (r.invDir().x >= 0.0) ? 0 : 3;
	}

	else
	{
		t0 = tymin;
		normal_in = (r.invDir().y >= 0.0) ? 1 : 4;
	}

	if (tzmin > t0)
	{
		t0 = tzmin;
		normal_in = (r.invDir().z >= 0.0) ? 2 : 5;
	}

	if (txmax < tymax)
	{
		t1 = txmax;
		normal_out = (r.invDir().x >= 0.0) ? 3 : 0;
	}

	else
	{
		t1 = tymax;
		normal_out = (r.invDir().y >= 0.0) ? 4 : 1;
	}

	if (tzmax < t1)
	{
		t1 = tzmax;
		normal_out = (r.invDir().z >= 0.0) ? 5 : 2;
	}

	if (t0 < t1 && t1 > 0.001f)
	{
		float d = 0.0f;
		if (t0 > 0.001f)
		{
			d = t0;
			h.setNormal(normals[normal_in]);
		}
		else
		{
			d = t1;
			h.setNormal(normals[normal_out]);
		}
		h.setDistToHit(d);
		h.setSuccess(true);
		h.setLocal(r.origin() + r.direction() * d);
		return h;
	}
	return h;
}

// Hit Bbox::intersects(Ray const& ray) {
// 	Hit hit{};
//   float tmin, tmax, tymin, tymax, tzmin, tzmax;
//   glm::ivec3 sign = (-glm::sign(ray.direction())) * 0.5f + 0.5f;
//   glm::fvec3 bounds[2] = {m_min, m_max};

// 	tmin = (bounds[sign[0]].x - ray.origin().x) * ray.invDir().x;
// 	tmax = (bounds[1-sign[0]].x - ray.origin().x) * ray.invDir().x;
// 	tymin = (bounds[sign[1]].y - ray.origin().y) * ray.invDir().y;
// 	tymax = (bounds[1-sign[1]].y - ray.origin().y) * ray.invDir().y;

// 	if ((tmin > tymax) || (tymin > tmax))
// 	return hit;

// 	if (tymin > tmin)
// 		tmin = tymin;
// 	if (tymax < tmax)
// 		tmax = tymax;

// 	tzmin = (bounds[sign[2]].z - ray.origin().z) * ray.invDir().z;
// 	tzmax = (bounds[1-sign[2]].z - ray.origin().z) * ray.invDir().z;

// 	if ((tmin > tzmax) || (tzmin > tmax))
// 	return hit;

// 	if (tzmin > tmin)
// 		tmin = tzmin;
// 	if (tzmax < tmax)
// 		tmax = tzmax;

// 	hit.setSuccess(true);
// 	return hit; 
// }