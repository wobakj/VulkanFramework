#ifndef HIT_HPP
#define HIT_HPP

#include "node/node.hpp"

class Hit
{
public:
	Hit();

	glm::vec3 const& getLocal() const;
	glm::vec3 const& getWorld() const;
	glm::vec3 const& getNormal() const;
	glm::vec3 const& getWorldNormal() const;
	Node* const& getNode() const;
	bool intersected() const;
	bool success();
	void setDistToHit(float const& d);
	void setLocal(glm::vec3 const& l);
	void setWorld(glm::vec3 const& w);
	void setNormal(glm::vec3 const& n);
	void setWorldNormal(glm::vec3 const& w);
	void setNode(Node* n);
	void setSuccess(bool success);

private:
	bool m_hit;
	Node * m_node;
	float m_dist_to_hit;
	glm::vec3 m_local;
	glm::vec3 m_world;
	glm::vec3 m_normal;
	glm::vec3 m_world_normal;
};

#endif
