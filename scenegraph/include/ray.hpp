#ifndef RAY_HPP
#define RAY_HPP

// #include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/type_precision.hpp>
#include <string>
#include <array>

class Ray
{
public:
	Ray();
	Ray(glm::fvec3 const& orig, glm::fvec3 const& dir);
	
	glm::fvec3 const& origin() const;
	glm::fvec3 const& direction() const;
	glm::fvec3 const& invDir() const;
	glm::uvec3 const& sign() const;
	Ray transform(glm::fmat4 const& mat) const;
private:
	glm::fvec3 m_origin;
	glm::fvec3 m_dir;
	glm::fvec3 m_inv_dir;
	glm::uvec3 m_sign;
};

#endif