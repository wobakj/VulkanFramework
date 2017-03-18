
#include <assert.h>
#include "bbox.hpp"
#include "node/node.hpp"

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

Bbox::~Bbox()
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