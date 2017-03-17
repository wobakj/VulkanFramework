#ifndef NODEGEOMETRY_HPP
#define NODEGEOMETRY_HPP

#include "node.hpp"

class GeometryNode : public Node
{
public:
	GeometryNode();
	GeometryNode(std::string const& name, glm::mat4 const& transform);

	void accept(NodeVisitor &v) override;
};

#endif

