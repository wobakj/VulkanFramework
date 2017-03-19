#include "node/node_model.hpp"
#include "hit.hpp"

ModelNode::ModelNode()
 :Node{std::string{}, glm::mat4{}}
{}

ModelNode::ModelNode(std::string const & name, std::string const& model, std::string const& transform)
 :Node{name, glm::fmat4{1.0f}}
 ,m_model{model}
 ,m_transform{transform}
{}

void ModelNode::accept(NodeVisitor &v)
{
	v.visit(this);
}