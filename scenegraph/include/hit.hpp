#ifndef HIT_HPP
#define HIT_HPP

#include "node.hpp"

class Hit
{
public:
	Hit();
	~Hit();

	glm::vec3 getLocal() const;
	glm::vec3 getWorld() const;
	glm::vec3 getNormal() const;
	glm::vec3 getWorldNormal() const;
	Node* getNode() const;

	void setLocal(glm::vec3 const& l);
	void setWorld(glm::vec3 const& w);
	void setNormal(glm::vec3 const& n);
	void setWorldNormal(glm::vec3 const& w);
	void setNode(Node* n);

private:
	Node* m_node;
	float m_dist_to_Hit;
	glm::vec3 m_local;
	glm::vec3 m_world;
	glm::vec3 m_normal;
	glm::vec3 m_world_normal;


};

#endif
