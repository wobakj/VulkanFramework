#ifndef VISITORNODE_HPP
#define VISITORNODE_HPP

class Node;
class GeometryNode;
class CameraNode;
class LightNode;
class ScreenNode;

class NodeVisitor
{
public:
	NodeVisitor();

	virtual void visit(Node* node);
	virtual void visit(GeometryNode* node);
	virtual void visit(CameraNode* node);
	virtual void visit(LightNode* node);
	virtual void visit(ScreenNode* node);
};

#endif