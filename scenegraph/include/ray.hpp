#ifndef RAY_HPP
#define RAY_HPP

#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <array>

class Ray
{
public:
	Ray();
	Ray(glm::vec4 orig, glm::vec4 dir);
	
	glm::vec4 getOrigin() const;
	glm::vec4 getDir() const;
	glm::vec4 getInvDir() const;
	std::array<bool, 3> getSign() const;

	void setOrigin(glm::vec4 const& o);
	void setDir(glm::vec4 const& d);
	void setInvDir(glm::vec4 const& d);
	void setSign(glm::vec4 const& d);


private:
	glm::vec4 m_origin;
	glm::vec4 m_dir;
	glm::vec4 m_inv_dir;
	std::array<bool, 3> m_sign;
	
};

#endif