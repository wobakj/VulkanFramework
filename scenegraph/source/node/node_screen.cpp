#include "node/node_screen.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>


ScreenNode::ScreenNode() {}

ScreenNode::ScreenNode(std::string const & name, glm::vec2 const & size, glm::mat4 const & transform) : 
Node(name, transform), m_size(size) {}


glm::mat4 ScreenNode::getScaledLocal() const
{
	glm::mat4 scaled = glm::scale(glm::vec3(m_size.x, m_size.y, 0.0f));
	return m_local * scaled;
}

glm::mat4 ScreenNode::getScaledWorld() const
{
	glm::mat4 scaled = glm::scale(glm::vec3(m_size.x, m_size.y, 0.0f));
	return m_world * scaled;
}

void ScreenNode::accept(NodeVisitor &v)
{
	v.visit(this);
}
