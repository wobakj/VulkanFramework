#ifndef VISITORRENDER_HPP
#define VISITORRENDER_HPP

#include "visitor_node.hpp"
#include "frustum.hpp"
#include <set>

class Node;
class GeometryNode;
class CameraNode;
class LightNode;
class ScreenNode;
class Frustum;

class RenderVisitor : public NodeVisitor
{
public:
	RenderVisitor();
	~RenderVisitor();

	void setFrustum(Frustum const& f);

	void visit(Node* node) override;
	void visit(GeometryNode* node) override;
	void visit(CameraNode* node) override;
	void visit(LightNode* node) override;
	void visit(ScreenNode* node) override;

private:
	std::set<GeometryNode*> m_toRender;
	Frustum m_frustum;

};

#endif
