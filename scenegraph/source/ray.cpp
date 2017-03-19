#include "ray.hpp"

Ray::Ray() {}

Ray::Ray(glm::fvec3 const& orig, glm::fvec3 const& dir)
 :m_origin{orig}
 ,m_dir{dir}
 ,m_inv_dir{1.0f / dir}
 ,m_sign{glm::sign(dir) * 0.5f + 0.5f}
{}

glm::fvec3 const& Ray::origin() const
{
	return m_origin;
}

glm::fvec3 const& Ray::direction() const
{
	return m_dir;
}

glm::fvec3 const& Ray::invDir() const
{
	return m_inv_dir;
}

glm::uvec3 const& Ray::sign() const
{
	return m_sign;
}

Ray Ray::transform(glm::fmat4 const& mat) const {
	return Ray{mat * glm::fvec4{origin(), 1.0f}, mat * glm::fvec4{direction(), 0.0f}};
}
