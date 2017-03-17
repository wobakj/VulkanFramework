#ifndef NODESCREEN_HPP
#define NODESCREEN_HPP

#include "node.hpp"

class ScreenNode : public Node
{
public:
	ScreenNode();
	ScreenNode(std::string const& name, glm::vec2 const& size, glm::mat4 const& transform);

	glm::mat4 getScaledLocal() const;
	glm::mat4 getScaledWorld() const;

	void accept(NodeVisitor &v) override;

private:
	glm::vec2 m_size;
};

#endif
