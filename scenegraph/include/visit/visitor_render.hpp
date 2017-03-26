#ifndef VISITORRENDER_HPP
#define VISITORRENDER_HPP

#include "visit/visitor_node.hpp"

#include "frustum.hpp"

#include <vector>

class Node;
class ModelNode;
class CameraNode;
class LightNode;
class ScreenNode;

class RenderVisitor : public NodeVisitor
{
public:
	RenderVisitor();

	//void setFrustum(Frustum const& f);

	void visit(Node* node) override;
	void visit(ModelNode* node) override;
	void visit(CameraNode* node) override;
	void visit(LightNode* node) override;
	void visit(ScreenNode* node) override;
	void visit(NavigationNode* node) override;

	std::vector<ModelNode const*> const& visibleNodes() {
		return m_toRender;
	}

private:
	std::vector<ModelNode const*> m_toRender;
	Frustum m_frustum;

};

#endif
