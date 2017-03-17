#ifndef FRUSTUM_HPP
#define FRUSTUM_HPP

#include <vector>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include "bbox.hpp"

class Frustum
{
private:
	glm::mat4 m_cam_transf;
	glm::mat4 m_screen_transf;
	glm::mat4 m_proj;
	glm::mat4 m_view;
	glm::mat4 m_transf;
	float m_near_clip;
	float m_far_clip;
	std::vector<glm::vec4> m_planes;

public:

	Frustum();
	~Frustum();
	void calculatePlanes(glm::mat4 const& cam_transf, glm::mat4 const& screen_transf, Frustum& Frustum);
	glm::mat4 perspective(float fov, glm::vec2 aspect, glm::mat4 cam_transf, glm::mat4 screen_transf, float m_near_clip, float m_far_clip);
	
	std::vector<glm::vec3> getFrustumCorners() const;
	std::vector<glm::vec4> getPlanes() const;
	glm::mat4 getCamTransf() const;
	glm::vec3 getCamPos() const;
	glm::mat4 getScreenTransf() const;
	glm::mat4 getProj() const;
	glm::mat4 getView() const;
	glm::mat4 getTransf() const;
	float getNear() const;
	float getFar() const;

	void setCamTransf(glm::mat4 cam_t);
	void setScreenTransf(glm::mat4 screen_t);
	void setProj(glm::mat4 proj);
	void setView(glm::mat4 view);
	void setNear(float n);
	void setFar(float f);

	bool intersects(Bbox &box);
	bool contains(glm::vec3 const& p);




	//::mat4 orthographic(float fov, glm::vec4 eye, glm::mat4 cam_transf, glm::mat4 screen_transf, float m_near_clip, float m_far_clip);
};

#endif

