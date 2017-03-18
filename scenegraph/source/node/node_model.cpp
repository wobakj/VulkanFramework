#include "node/node_model.hpp"

ModelNode::ModelNode()
 :Node{std::string{}, glm::mat4{}}, m_oriented_box{}
{}

ModelNode::ModelNode(std::string const & name, std::string const& model, std::string const& transform)
 :Node{name, glm::fmat4{1.0f}}
 ,m_model{model}
 ,m_transform{transform},
 m_oriented_box{}
{}

Bbox const& ModelNode::getOrientedBox() const
{
	return m_oriented_box;
}

void ModelNode::setOrientedBoxPoints(glm::vec3 const& min, glm::vec3 const& max)
{
	m_oriented_box.setMin(min);
	m_oriented_box.setMax(max);
}

void ModelNode::setOrientedBoxTransform(glm::mat4 const& transform)
{
	m_oriented_box.transformBox(transform);
}

void ModelNode::accept(NodeVisitor &v)
{
	v.visit(this);
}