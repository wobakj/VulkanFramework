#include "node/node_ray.hpp"

RayNode::RayNode()
 :Node{std::string{}, glm::mat4{}}
{}

RayNode::RayNode(std::string const & name, Ray const& ray)
 :Node{name, glm::fmat4{1.0f}}
 ,m_ray{ray}
{}

void RayNode::accept(NodeVisitor &v)
{
	v.visit(this);
}

Ray RayNode::worldRay() const {
  return m_ray.transform(getWorld());
}
