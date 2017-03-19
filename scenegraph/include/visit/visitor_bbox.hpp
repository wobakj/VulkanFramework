#ifndef VISITORBBOX_HPP
#define VISITORBBOX_HPP

#include "visit/visitor_node.hpp"
#include "bbox.hpp"

class ApplicationInstance;

class BboxVisitor : public NodeVisitor
{
public:
	BboxVisitor(ApplicationInstance& instance);

	void visit(Node* node) override;
	void visit(ModelNode* node) override;
	void visit(CameraNode* node) override;
	void visit(LightNode* node) override;
	void visit(ScreenNode* node) override;

private:
  ApplicationInstance* m_instance;
};

#endif