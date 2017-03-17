#include "node_navigation.hpp"



NavigationNode::NavigationNode()
 :Node() {}

void NavigationNode::accept(NodeVisitor & v)
{
	v.visit(this);
}
