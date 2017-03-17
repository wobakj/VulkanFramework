#include "visitor_node.hpp"

#include "node.hpp"
#include "node_geometry.hpp"
#include "node_light.hpp"
#include "node_camera.hpp"
#include "node_screen.hpp"


NodeVisitor::NodeVisitor()
{
}


NodeVisitor::~NodeVisitor()
{
}

void NodeVisitor::visit(node * node)
{
}

void NodeVisitor::visit(geometryNode * node)
{
}

void NodeVisitor::visit(cameraNode * node)
{
}

void NodeVisitor::visit(lightNode * node)
{
}

void NodeVisitor::visit(screenNode * node)
{
}
