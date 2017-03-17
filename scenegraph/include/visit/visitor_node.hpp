#ifndef VISITOR_NODE_HPP
#define VISITOR_NODE_HPP

class Node;
class GeometryNode;
class CameraNode;
class LightNode;
class ScreenNode;

class NodeVisitor
{
public:
	NodeVisitor() {};

	virtual void visit(Node* node) = 0;
	virtual void visit(GeometryNode* node) = 0;
	virtual void visit(CameraNode* node) = 0;
	virtual void visit(LightNode* node) = 0;
	virtual void visit(ScreenNode* node) = 0;
};

#endif