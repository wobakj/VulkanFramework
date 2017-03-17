#ifndef VISITORRENDER_HPP
#define VISITORRENDER_HPP

#include "visitor_node.hpp"

#include "frustum.hpp"

#include <vector>

class Node;
class GeometryNode;
class CameraNode;
class LightNode;
class ScreenNode;

class RenderVisitor : public NodeVisitor
{
public:
	RenderVisitor();

	// void setFrustum(Frustum const& f);

	void visit(Node* node) override;
	void visit(GeometryNode* node) override;
	void visit(CameraNode* node) override;
	void visit(LightNode* node) override;
	void visit(ScreenNode* node) override;

	std::vector<GeometryNode const*> const& visibleNodes() {
		return m_toRender;
	}

private:
	std::vector<GeometryNode const*> m_toRender;
	// Frustum m_frustum;

};

#endif
