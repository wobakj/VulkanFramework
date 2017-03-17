#include "node_light.hpp"



LightNode::LightNode() {}

LightNode::LightNode(std::string &name, glm::mat4 const& transform, glm::vec4 &color, float const& brightness) : 
Node(name, transform), m_color(color), m_brightness(brightness) { m_name = name; }

glm::vec4 const& LightNode::getColor() const
{
	return m_color;
}

void LightNode::accept(NodeVisitor &v)
{
	v.visit(this);
}


