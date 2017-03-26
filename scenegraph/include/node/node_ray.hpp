#ifndef NODE_RAY_HPP
#define NODE_RAY_HPP


#include "node.hpp"

#include "ray.hpp"
#include "visit/visitor_node.hpp"

class RayNode : public Node
{
 public:
	RayNode();
	RayNode(std::string const& name, Ray const& ray = Ray{glm::fvec3{0.0f}, glm::fvec3{0.0f, 0.0f, -1.0f}});

	void accept(NodeVisitor &v) override;
  Ray worldRay() const;

 private:
  Ray m_ray;
};

#endif

