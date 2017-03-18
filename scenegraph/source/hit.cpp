#include "hit.hpp"


Hit::Hit() {}

Hit::~Hit() {}

glm::vec3 const& Hit::getLocal() const
{
	return m_local;
}

glm::vec3 const& Hit::getWorld() const
{
	return m_world;
}

glm::vec3 const& Hit::getNormal() const
{
	return m_normal;
}

glm::vec3 const& Hit::getWorldNormal() const
{
	return m_world_normal;
}

Node * const& Hit::getNode() const
{
	return m_node;
}

void Hit::setDistToHit(float const & d)
{
	m_dist_to_hit = d;
}

void Hit::setLocal(glm::vec3 const & l)
{
	m_local = l;
}

void Hit::setWorld(glm::vec3 const & w)
{
	m_world = w;
}

void Hit::setNormal(glm::vec3 const & n)
{
	m_normal = n;
}

void Hit::setWorldNormal(glm::vec3 const & w)
{
	m_world_normal = w;
}

void Hit::setNode(Node* n)
{
	m_node = n;
}
