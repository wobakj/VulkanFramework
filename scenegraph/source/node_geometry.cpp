#include "node_geometry.hpp"

GeometryNode::GeometryNode()
 :Node{std::string{}, glm::mat4{}}
{}

GeometryNode::GeometryNode(std::string const & name, std::string const& model, std::string const& transform)
 :Node{name, glm::fmat4{1.0f}}
 ,m_model{model}
 ,m_transform{transform}
{}

void GeometryNode::accept(NodeVisitor &v)
{
	v.visit(this);
}