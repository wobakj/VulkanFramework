#include "frustum.hpp"

#include <glm/matrix.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#define PI    3.14159265358979323846f

Frustum::Frustum() {}

// void Frustum::calculatePlanes(glm::mat4 const & cam_transf, glm::mat4 const & screen_transf, Frustum & f)
// {
// 	glm::mat4 view = screen_transf;
// 	view[3][0] = 0.f;
// 	view[3][1] = 0.f;
// 	view[3][2] = 0.f;
// 	view[3][3] = 1.f;

// 	f.m_cam_transf = cam_transf;
// 	f.m_screen_transf = screen_transf;
// 	glm::vec3 campos = glm::vec3(f.m_cam_transf[3][0], f.m_cam_transf[3][1], f.m_cam_transf[3][2]);
// 	view = glm::translate(view, campos);

// 	f.m_view = glm::inverse(view);

// 	f.m_transf = f.m_proj * f.m_view;

// 	//left plane
// 	f.m_planes[0] = glm::vec4(f.m_transf[0][3] + f.m_transf[0][0],
// 		f.m_transf[1][3] + f.m_transf[1][0],
// 		f.m_transf[2][3] + f.m_transf[2][0],
// 		f.m_transf[3][3] + f.m_transf[3][0]);

// 	//right plane
// 	f.m_planes[1] = glm::vec4(f.m_transf[0][3] - f.m_transf[0][0],
// 		f.m_transf[1][3] - f.m_transf[1][0],
// 		f.m_transf[2][3] - f.m_transf[2][0],
// 		f.m_transf[3][3] - f.m_transf[3][0]);

// 	//bottom plane
// 	f.m_planes[2] = glm::vec4(f.m_transf[0][3] + f.m_transf[0][1],
// 		f.m_transf[1][3] + f.m_transf[1][1],
// 		f.m_transf[2][3] + f.m_transf[2][1],
// 		f.m_transf[3][3] + f.m_transf[3][1]);

// 	//top plane
// 	f.m_planes[3] = glm::vec4(f.m_transf[0][3] - f.m_transf[0][1],
// 		f.m_transf[1][3] - f.m_transf[1][1],
// 		f.m_transf[2][3] - f.m_transf[2][1],
// 		f.m_transf[3][3] - f.m_transf[3][1]);

// 	//near plane
// 	f.m_planes[4] = glm::vec4(f.m_transf[0][3] + f.m_transf[0][2],
// 		f.m_transf[1][3] + f.m_transf[1][2],
// 		f.m_transf[2][3] + f.m_transf[2][2],
// 		f.m_transf[3][3] + f.m_transf[3][2]);

// 	//far plane
// 	f.m_planes[5] = glm::vec4(f.m_transf[0][3] - f.m_transf[0][2],
// 		f.m_transf[1][3] - f.m_transf[1][2],
// 		f.m_transf[2][3] - f.m_transf[2][2],
// 		f.m_transf[3][3] - f.m_transf[3][2]);

// 	//normalize
// 	for (unsigned int i = 0; i < 6; ++i)
// 	{
// 		float m = glm::length(glm::vec3(f.m_planes[i].x, f.m_planes[i].y, f.m_planes[i].z));
// 		f.m_planes[i] = f.m_planes[i] / m;
// 	}
// }

// glm::mat4 Frustum::perspective(float fov, glm::vec2 aspect, glm::mat4 cam_transf, glm::mat4 screen_transf, float m_near_clip, float m_far_clip)
// {
// 	Frustum f = Frustum();
// 	float ratio = aspect.x / aspect.y;
// 	f.m_proj = glm::perspective(fov, ratio, m_near_clip, m_far_clip);
// 	return f.m_proj;
// }

// std::vector<glm::vec3> Frustum::getFrustumCorners() const
// {
// 	std::vector<glm::vec4> intersections(8);
// 	std::vector<glm::vec3> corners(8);

// 	glm::mat4 world = glm::inverse(m_transf);

// 	intersections[0] = world * glm::vec4(-1, -1, -1, 1);
// 	intersections[1] = world * glm::vec4(-1, -1, 1, 1);
// 	intersections[2] = world * glm::vec4(-1, 1, -1, 1);
// 	intersections[3] = world * glm::vec4(-1, 1, 1, 1);
// 	intersections[4] = world * glm::vec4(1, -1, -1, 1);
// 	intersections[5] = world * glm::vec4(1, -1, 1, 1);
// 	intersections[6] = world * glm::vec4(1, 1, -1, 1);
// 	intersections[7] = world * glm::vec4(1, 1, 1, 1);

// 	for (unsigned int i = 0; i < 8; ++i) {
// 		corners[i] = glm::vec3(intersections[i].x, intersections[i].y, intersections[i].z);
// 	}
// 	return corners;
// }

// std::vector<glm::vec4> const& Frustum::getPlanes() const
// {
// 	return m_planes;
// }

// glm::mat4 const& Frustum::getCamTransf() const
// {
// 	return m_cam_transf;
// }

// glm::vec3 Frustum::getCamPos() const
// {
// 	return glm::vec3(m_cam_transf[3].x, m_cam_transf[3].y, m_cam_transf[3].z);
// }

// glm::mat4 const& Frustum::getScreenTransf() const
// {
// 	return m_screen_transf;
// }

// glm::mat4 const& Frustum::getProj() const
// {
// 	return m_proj;
// }

// glm::mat4 const& Frustum::getView() const
// {
// 	return m_view;
// }

// glm::mat4 const& Frustum::getTransf() const
// {
// 	return m_transf;
// }

// float const& Frustum::getNear() const
// {
// 	return m_near_clip;
// }

// float const& Frustum::getFar() const
// {
// 	return m_far_clip;
// }

// void Frustum::setCamTransf(glm::mat4 cam_t)
// {
// 	m_cam_transf = cam_t;
// }

// void Frustum::setScreenTransf(glm::mat4 screen_t)
// {
// 	m_screen_transf = screen_t;
// }

// void Frustum::setProj(glm::mat4 proj)
// {
// 	m_proj = proj;
// }

// void Frustum::setView(glm::mat4 view)
// {
// 	m_view = view;
// }

// void Frustum::setNear(float n)
// {
// 	m_near_clip = n;
// }

// void Frustum::setFar(float f)
// {
// 	m_far_clip = f;
// }

// bool Frustum::intersects(Bbox const& box)
// {
// 	for (unsigned int i = 0; i < 6; ++i)
// 	{
// 		glm::vec3 p_pos = box.getPosVertex(m_planes[i]);
// 		float pos_dist = m_planes[i].x * p_pos.x + m_planes[i].y * p_pos.y + m_planes[i].z * p_pos.z +
// 			m_planes[i].t;
// 		if (pos_dist < 0) return false;

// 		return true;
// 	}
// 	return false;
// }

// bool Frustum::contains(glm::vec3 const & p)
// {
// 	for (unsigned int i = 0; i < 6; ++i)
// 	{
// 		float pt_dist = m_planes[i].x * p.x + m_planes[i].y * p.y + m_planes[i].z * p.z +
// 			m_planes[i].t;
// 		if (pt_dist < 0) return false;
// 	}
// 	return true;
// }


