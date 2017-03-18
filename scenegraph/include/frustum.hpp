#ifndef FRUSTUM_HPP
#define FRUSTUM_HPP

#include <vector>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include "bbox.hpp"

class Frustum
{
private:
	glm::fmat4 m_cam_transf{1.0f};
	glm::fmat4 m_screen_transf{1.0f};
	glm::fmat4 m_proj{1.0f};
	// float m_transf2[4][4];
	// double dummy[3][2];
	glm::fmat4 m_view{1.0f};
	glm::fmat4 m_transf{1.0f};
	// float m_near_clip;
	// float m_far_clip;
	// std::vector<glm::vec4> m_planes;

public:
	Frustum();
	// void calculatePlanes(glm::mat4 const& cam_transf, glm::mat4 const& screen_transf, Frustum& f);
	// glm::mat4 perspective(float fov, glm::vec2 aspect, glm::mat4 cam_transf, glm::mat4 screen_transf, float m_near_clip, float m_far_clip);
	
	// std::vector<glm::vec3> getFrustumCorners() const;
	// std::vector<glm::vec4> const& getPlanes() const;
	// glm::mat4 const& getCamTransf() const;
	// glm::vec3 getCamPos() const;
	// glm::mat4 const& getScreenTransf() const;
	// glm::mat4 const& getProj() const;
	// glm::mat4 const& getView() const;
	// glm::mat4 const& getTransf() const;
	// float const& getNear() const;
	// float const& getFar() const;

	// void setCamTransf(glm::mat4 cam_t);
	// void setScreenTransf(glm::mat4 screen_t);
	// void setProj(glm::mat4 proj);
	// void setView(glm::mat4 view);
	// void setNear(float n);
	// void setFar(float f);

	// bool intersects(Bbox const& box);
	// bool contains(glm::vec3 const& p);

	//::mat4 orthographic(float fov, glm::vec4 eye, glm::mat4 cam_transf, glm::mat4 screen_transf, float m_near_clip, float m_far_clip);
};

#endif

