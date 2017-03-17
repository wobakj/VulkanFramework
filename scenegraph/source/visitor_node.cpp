#include "visitor_node.hpp"

#include "node.hpp"
#include "node_geometry.hpp"
#include "node_light.hpp"
#include "node_camera.hpp"
#include "node_screen.hpp"


NodeVisitor::NodeVisitor()
{
}

void NodeVisitor::visit(Node * node)
{
}

void NodeVisitor::visit(GeometryNode * node)
{
}

void NodeVisitor::visit(CameraNode * node)
{
}

void NodeVisitor::visit(LightNode * node)
{
}

void NodeVisitor::visit(ScreenNode * node)
{
}
