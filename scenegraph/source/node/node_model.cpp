#include "node/node_model.hpp"
#include "hit.hpp"

ModelNode::ModelNode()
 :Node{std::string{}, glm::mat4{}}, m_oriented_box{}
{}

ModelNode::ModelNode(std::string const & name, std::string const& model, std::string const& transform)
 :Node{name, glm::fmat4{1.0f}}
 ,m_model{model}
 ,m_transform{transform},
 m_oriented_box{}
{}

Bbox const& ModelNode::getOrientedBox() const
{
	return m_oriented_box;
}

std::shared_ptr<Hit> ModelNode::orientedBoxIntersectsRay(Ray const& r)
{
	float tMin = 0.0f;
	float tMax = 100000.0f;

	std::shared_ptr<Hit> h = std::shared_ptr<Hit>(new Hit());
	glm::mat4 m_world = getWorld();
	glm::vec3 obox_worldpos(m_world[3][0], m_world[3][1], m_world[3][2]);
	glm::vec4 invPoint = glm::vec4(m_oriented_box.getMin(), 1.0f) * glm::inverse(m_world);
	glm::vec3 aabb_min = glm::vec3(invPoint.x, invPoint.y, invPoint.z);
	invPoint = glm::vec4(m_oriented_box.getMax(), 1.0f) * glm::inverse(m_world);
	glm::vec3 aabb_max = glm::vec3(invPoint.x, invPoint.y, invPoint.z);

	glm::vec3 delta = obox_worldpos - glm::vec3(r.getOrigin().x, r.getOrigin().y, r.getOrigin().z);

	// Test intersection with the 2 planes perpendicular to the OBB's X axis
	{

		glm::vec3 x_axis(m_world[0][0], m_world[0][1], m_world[0][2]);
		float e = glm::dot(x_axis, delta);
		float f = glm::dot(glm::vec3(r.getDir().x, r.getDir().y, r.getDir().z), x_axis);

		if (std::fabs(f) > 0.001f)
		{

			float t1 = (e + aabb_min.x) / f; // Intersection with the left plane
			float t2 = (e + aabb_max.x)/f; // Intersection with the right plane

			if (t1 > t2){
				float w = t1; t1 = t2;t2 = w; // swap t1 and t2
			}

			// tMax is the nearest "far" intersection (amongst the X,Y and Z planes pairs)
			if ( t2 < tMax )
				tMax = t2;
			// tMin is the farthest "near" intersection (amongst the X,Y and Z planes pairs)
			if ( t1 > tMin )
				tMin = t1;

			// And here's the trick :
			// If "far" is closer than "near", then there is NO intersection.
			// See the images in the tutorials for the visual explanation.
			if (tMax < tMin) 
			{
				h->setNode(nullptr);
				return h;
			}

		}
		else
		{ // Rare case : the ray is almost parallel to the planes, so they don't have any "intersection"
			if(-e + aabb_min.x > 0.0f || -e + aabb_max.x < 0.0f)
			{
				h->setNode(nullptr);
				return h;
			}
		}
	}

	{
		glm::vec3 y_axis(m_world[1][0], m_world[1][1], m_world[1][2]);
		float e = glm::dot(y_axis, delta);
		float f = glm::dot(glm::vec3(r.getDir().x, r.getDir().y, r.getDir().z), y_axis);

		if (std::fabs(f) > 0.001f)
		{

			float t1 = (e + aabb_min.y) / f;
			float t2 = (e + aabb_max.y) / f;

			if (t1 > t2)
			{
				float w = t1; t1 = t2; t2 = w;
			}

			if ( t2 < tMax )
				tMax = t2;
			if ( t1 > tMin )
				tMin = t1;
			if (tMin > tMax)
			{
				h->setNode(nullptr);
				return h;
			}

		}
		else
		{
			if(-e + aabb_min.y > 0.0f || -e + aabb_max.y < 0.0f)
			{
				h->setNode(nullptr);
				return h;
			}
		}
	}

	{
		glm::vec3 z_axis(m_world[2][0], m_world[2][1], m_world[2][2]);
		float e = glm::dot(z_axis, delta);
		float f = glm::dot(glm::vec3(r.getDir().x, r.getDir().y, r.getDir().z), z_axis);

		if (std::fabs(f) > 0.001f)
		{
			float t1 = (e + aabb_min.z) / f;
			float t2 = (e + aabb_max.z) / f;

			if (t1 > t2)
			{
				float w = t1; t1 = t2; t2 = w;
			}

			if ( t2 < tMax )
				tMax = t2;
			if ( t1 > tMin )
				tMin = t1;
			if (tMin > tMax)
			{
				h->setNode(nullptr);
				return h;
			}

		}
		else
		{
			if(-e + aabb_min.z > 0.0f || -e+aabb_max.z < 0.0f)
			{
				h->setNode(nullptr);
				return h;
			}
		}
	}
	h->setNode(this);
	h->setDistToHit(tMin);
	return h;
}


void ModelNode::setOrientedBoxPoints(glm::vec3 const& min, glm::vec3 const& max)
{
	m_oriented_box.setMin(min);
	m_oriented_box.setMax(max);
}

void ModelNode::setOrientedBoxTransform(glm::mat4 const& transform)
{
	m_oriented_box.transformBox(transform);
}

void ModelNode::accept(NodeVisitor &v)
{
	v.visit(this);
}