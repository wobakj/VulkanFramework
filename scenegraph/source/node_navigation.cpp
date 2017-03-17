#include "node_navigation.hpp"



NavigationNode::NavigationNode()
 :Node()
{
}


NavigationNode::~NavigationNode()
{
}

void NavigationNode::accept(NodeVisitor & v)
{
	v.visit(this);
}
