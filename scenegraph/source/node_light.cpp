#include "node_light.hpp"



LightNode::LightNode() {}

LightNode::LightNode(std::string &name, glm::mat4 const& transform, glm::vec4 &color, float const& brightness) : 
Node(name, transform), m_color(color), m_brightness(brightness) { m_name = name; }

void LightNode::accept(NodeVisitor &v)
{
	v.visit(this);
}

glm::vec4 LightNode::getColor() const
{
	return m_color;
}
