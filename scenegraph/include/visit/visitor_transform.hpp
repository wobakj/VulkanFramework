#ifndef VISITORTRANSFORM_HPP
#define VISITORTRANSFORM_HPP

#include "visit/visitor_node.hpp"
#include <glm/mat4x4.hpp>

class ApplicationInstance;

class TransformVisitor : public NodeVisitor
{
public:
	TransformVisitor(ApplicationInstance& instance);

	glm::mat4 const& getTransform() const;

	void visit(Node* node) override;
	void visit(ModelNode* node) override;
	void visit(CameraNode* node) override;
	void visit(LightNode* node) override;
	void visit(ScreenNode* node) override;

private:
	glm::mat4 m_transf;
  ApplicationInstance* m_instance;
};

#endif
