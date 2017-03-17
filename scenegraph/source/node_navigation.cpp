#include "node_Navigation.hpp"



NavigationNode::NavigationNode() : node()
{
}


NavigationNode::~NavigationNode()
{
}

void NavigationNode::accept(NodeVisitor & v)
{
	v.visit(this);
}
