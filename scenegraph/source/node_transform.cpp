#include "node_transform.hpp"



TransformNode::TransformNode() : Node()
{
}

TransformNode::TransformNode(std::string const & name, glm::mat4 const & transf) : Node(name, transf)
{
}


TransformNode::~TransformNode()
{
}

void TransformNode::accept(NodeVisitor & v)
{
	v.visit(this);
}

