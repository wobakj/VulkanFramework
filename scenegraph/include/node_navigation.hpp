#ifndef NODENAVIGATION_HPP
#define NODENAVIGATION_HPP

#include "node.hpp"

class NavigationNode : public Node
{
public:
	NavigationNode();

	void accept(NodeVisitor &v) override;
};

#endif
