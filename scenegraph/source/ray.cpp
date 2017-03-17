#include "ray.hpp"



Ray::Ray() {}

Ray::Ray(glm::vec4 orig, glm::vec4 dir) {}

glm::vec4 const& Ray::getOrigin() const
{
	return m_origin;
}


glm::vec4 const& Ray::getDir() const
{
	return m_dir;
}

glm::vec4 const& Ray::getInvDir() const
{
	return m_inv_dir;
}

std::array<bool, 3> const& Ray::getSign() const
{
	return m_sign;
}

void Ray::setOrigin(glm::vec4 const & o)
{
	m_origin = o;
}

void Ray::setDir(glm::vec4 const & d)
{
	m_dir = d;
}

void Ray::setInvDir(glm::vec4 const & d)
{
	m_inv_dir = glm::vec4(1.0f / d.x, 1.0f / d.y, 1.0f / d.z, 1.0f / d.t);
}

void Ray::setSign(glm::vec4 const & d)
{
	m_sign[0] = (m_inv_dir.x < 0.0f);
	m_sign[1] = (m_inv_dir.y < 0.0f);
	m_sign[2] = (m_inv_dir.z < 0.0f);
}
