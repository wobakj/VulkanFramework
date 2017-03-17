#include "node/node_light.hpp"

LightNode::LightNode() {}

LightNode::LightNode(std::string const& name, std::string const& light)
 :Node{name, glm::fmat4{}}
 ,m_light(light)
{}

void LightNode::accept(NodeVisitor &v)
{
	v.visit(this);
}