#ifndef NODEGEOMETRY_HPP
#define NODEGEOMETRY_HPP

#include "Node.hpp"

class GeometryNode : public Node
{
public:
	GeometryNode();
	GeometryNode(std::string const& name, glm::mat4 const& transform);
	~GeometryNode();

	void accept(NodeVisitor &v) override;
};

#endif

