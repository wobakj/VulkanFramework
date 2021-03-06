#ifndef BBOX_HPP
#define BBOX_HPP

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <utility>

class Hit;
class Ray;

class Bbox {
private:
	void setMin(glm::vec3 const& min);
	void setMax(glm::vec3 const& max);
	glm::vec3 m_min;
	glm::vec3 m_max;
public:
	Bbox();
	Bbox(glm::vec3 const& pmin, glm::vec3 const& pmax);

	void transform(glm::mat4 const& transform);
	void join(Bbox const& b);

	glm::vec3 const& getMin() const;
	glm::vec3 const& getMax() const;
	bool isEmpty() const;
	std::pair<glm::vec3, glm::vec3> corners() const;
	glm::vec3 center() const;
	
	bool contains(glm::vec3 const& p) const;
	bool intersects(Bbox const &box) const;
  Hit intersects(Ray const& r);

	glm::vec3 getPosVertex(glm::vec4 &normal) const;
	glm::vec3 getNegVertex(glm::vec4 &normal) const;
};

#endif