#ifndef VISITORTRANSFORM_HPP
#define VISITORTRANSFORM_HPP

#include "visit/visitor_node.hpp"
#include <glm/mat4x4.hpp>

class ApplicationInstance;

class TransformVisitor : public NodeVisitor
{
public:
	TransformVisitor(ApplicationInstance& instance);

	void visit(Node* node) override;
	void visit(ModelNode* node) override;
	void visit(CameraNode* node) override;
	void visit(LightNode* node) override;
	void visit(ScreenNode* node) override;

private:
  ApplicationInstance* m_instance;
};

#endif
