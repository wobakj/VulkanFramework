#ifndef BBOX_HPP
#define BBOX_HPP

#include <glm/vec3.hpp>
#include <utility>

class Bbox
{
private:
	glm::vec3 m_min;
	glm::vec3 m_max;
public:
	Bbox();
	Bbox(glm::vec3 const& p);
	Bbox(glm::vec3 const& pmin, glm::vec3 const& pmax);
	~Bbox();

	glm::vec3 getMin() const;
	glm::vec3 getMax() const;

	void setMin(float x, float y, float z);
	void setMax(float x, float y, float z);

	bool isEmpty() const;
	std::pair<glm::vec3, glm::vec3> corners() const;
	bool contains(glm::vec3 &p) const;
	bool intersects(Bbox const &box) const;
	glm::vec3 center() const;

	glm::vec3 getPosVertex(glm::vec4 &normal) const;
	glm::vec3 getNegVertex(glm::vec4 &normal) const;
};

#endif