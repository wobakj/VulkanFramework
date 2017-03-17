#ifndef VISITORTRANSFORM_HPP
#define VISITORTRANSFORM_HPP

#include "visitor_node.hpp"
#include <glm/mat4x4.hpp>


class TransformVisitor : public NodeVisitor
{
public:
	TransformVisitor();
	~TransformVisitor();

	glm::mat4 const& getTransform() const;

	void visit(Node* node) override;
	void visit(GeometryNode* node) override;
	void visit(CameraNode* node) override;
	void visit(LightNode* node) override;
	void visit(ScreenNode* node) override;

private:
	glm::mat4 m_transf;
};

#endif
