#ifndef VISITOR_NODE_HPP
#define VISITOR_NODE_HPP

class Node;
class ModelNode;
class CameraNode;
class LightNode;
class ScreenNode;
class NavigationNode;

class NodeVisitor
{
public:
	NodeVisitor() {};

	virtual void visit(Node* node) = 0;
	virtual void visit(ModelNode* node) = 0;
	virtual void visit(CameraNode* node) = 0;
	virtual void visit(LightNode* node) = 0;
  virtual void visit(ScreenNode* node) = 0;
	virtual void visit(NavigationNode* node) = 0;
};

#endif