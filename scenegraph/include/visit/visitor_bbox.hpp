#ifndef VISITORBBOX_HPP
#define VISITORBBOX_HPP

#include <memory>

#include "visit/visitor_node.hpp"
#include "bbox.hpp"
#include "node_geometry.hpp"
#include "node_light.hpp"
#include "node_screen.hpp"
#include "node_camera.hpp"
#include "frustum.hpp"

class BboxVisitor : public NodeVisitor
{
public:
	BboxVisitor();
	~BboxVisitor();

	Bbox const& getBox() const;

	void visit(Node* node) override;
	void visit(GeometryNode* node) override;
	void visit(CameraNode* node) override;
	void visit(LightNode* node) override;
	void visit(ScreenNode* node) override;

private:
	Bbox m_box;
};

#endif