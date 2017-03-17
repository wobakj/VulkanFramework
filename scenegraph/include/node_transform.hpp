#ifndef NODETRANSFORM_HPP
#define NODETRANSFORM_HPP

#include "node.hpp"

class TransformNode : public Node
{
public:
	TransformNode();
	TransformNode(std::string const& name, glm::mat4 const& transf);
	~TransformNode();

	void accept(NodeVisitor &v) override;
};

#endif