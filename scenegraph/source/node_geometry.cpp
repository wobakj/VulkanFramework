#include "node_geometry.hpp"



GeometryNode::GeometryNode() : Node(std::string(), glm::mat4())
{
}


GeometryNode::GeometryNode(std::string const & name, glm::mat4 const & transform) : Node(name, transform)
{
}

GeometryNode::~GeometryNode()
{
}

void GeometryNode::accept(NodeVisitor &v)
{
	v.visit(this);
}
