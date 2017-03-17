#ifndef VISITORTRANSFORM_HPP
#define VISITORTRANSFORM_HPP

#include "visitor_node.hpp"


class TransformVisitor : public NodeVisitor
{
public:
	TransformVisitor();
	~TransformVisitor();

	void visit(Node* node) override;
	void visit(GeometryNode* node) override;
	void visit(CameraNode* node) override;
	void visit(LightNode* node) override;
	void visit(ScreenNode* node) override;

private:
	glm::mat4 m_transf;
};

#endif
