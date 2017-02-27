#include "model_loader.hpp"

#include "bvh.h"
#include "lod_stream.h"

#include "tiny_obj_loader.h"

// use floats and med precision operations
#include <glm/gtc/type_precision.hpp>
#include <glm/geometric.hpp>

#include <iostream>


// struct Vertex {
//   Vertex(glm::vec3 const& position,
//          glm::vec3 const& normal,
//          glm::vec2 const& texCoord,
//          int materialId = -1)
//       : position(position),
//         normal(normal),
//         texCoord(texCoord),
//         materialId(materialId) {}

//   glm::vec3 position;
//   glm::vec3 normal;
//   glm::vec2 texCoord;
// };

// struct Shape {
//   std::string name;
//   unsigned int indexOffset;
//   unsigned int numIndices;
//   int materialId = -1;
// };

namespace model_loader {

void generate_normals(tinyobj::mesh_t& model);

std::vector<glm::fvec3> generate_tangents(tinyobj::mesh_t const& model_t);

model_t obj(std::string const& name, model_t::attrib_flag_t import_attribs){
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string err = tinyobj::LoadObj(shapes, materials, name.c_str());

  if (!err.empty()) {
    if (err[0] == 'W' && err[1] == 'A' && err[2] == 'R') {
      std::cerr << "tinyobjloader: " << err << std::endl;    
    }
    else {
      throw std::logic_error("tinyobjloader: " + err);    
    }
  }

  model_t::attrib_flag_t attributes{model_t::POSITION | import_attribs};

  std::vector<float> vertex_data;
  std::vector<unsigned> triangles;

  unsigned vertex_offset = 0;

  for (auto& shape : shapes) {
    tinyobj::mesh_t& curr_mesh = shape.mesh;
    // prevent MSVC warning due to Win BOOL implementation
    bool has_normals = (import_attribs & model_t::NORMAL) != 0;
    if(has_normals) {
      // generate normals if necessary
      if (curr_mesh.normals.empty()) {
        generate_normals(curr_mesh);
      }
    }

    bool has_uvs = (import_attribs & model_t::TEXCOORD) != 0;
    if(has_uvs) {
      if (curr_mesh.texcoords.empty()) {
        has_uvs = false;
        attributes ^= model_t::TEXCOORD;
        std::cerr << "Shape has no texcoords" << std::endl;
      }
    }

    bool has_tangents = import_attribs & model_t::TANGENT;
    std::vector<glm::fvec3> tangents;
    if (has_tangents) {
      if (!has_uvs) {
        has_tangents = false;
        attributes ^= model_t::TANGENT;
        std::cerr << "Shape has no texcoords" << std::endl;
      }
      else {
        tangents = generate_tangents(curr_mesh);
      }
    }

    // push back vertex attributes
    for (unsigned i = 0; i < curr_mesh.positions.size() / 3; ++i) {
      vertex_data.push_back(curr_mesh.positions[i * 3]);
      vertex_data.push_back(curr_mesh.positions[i * 3 + 1]);
      vertex_data.push_back(curr_mesh.positions[i * 3 + 2]);

      if (has_normals) {
        vertex_data.push_back(curr_mesh.normals[i * 3]);
        vertex_data.push_back(curr_mesh.normals[i * 3 + 1]);
        vertex_data.push_back(curr_mesh.normals[i * 3 + 2]);
      }

      if (has_uvs) {
        vertex_data.push_back(curr_mesh.texcoords[i * 2]);
        vertex_data.push_back(curr_mesh.texcoords[i * 2 + 1]);
      }

      if (has_tangents) {
        vertex_data.push_back(tangents[i].x);
        vertex_data.push_back(tangents[i].y);
        vertex_data.push_back(tangents[i].z);
      }
    }

    // add triangles
    for (unsigned i = 0; i < curr_mesh.indices.size(); ++i) {
      triangles.push_back(vertex_offset + curr_mesh.indices[i]);
    }

    vertex_offset += unsigned(curr_mesh.positions.size() / 3);
  }
  if(vertex_data.empty()) throw std::exception();
  return model_t{vertex_data, attributes, triangles};
}

// model_t obj(std::string const& name, model_t::attrib_flag_t import_attribs) {
//   tinyobj::attrib_t attrib;
//   std::vector<tinyobj::shape_t> shapes;
//   std::string err;

//   bool ret =
//       tinyobj::LoadObj(&attrib, &shapes, &m_materials, &err, filename.c_str());
//   if (!ret)
//     throw std::runtime_error("Failed to load mesh from '" + filename + "'");
//   else if (!err.empty())
//     throw std::runtime_error("Failed to load mesh from '" + filename + "'");

//   std::map<tinyobj::index_t, uint32_t> vertexToIndexMap;
//   std::vector<Vertex> vertices;
//   std::vector<uint32_t> indices;

//   // iterate over shapes
//   for (size_t s = 0; s < shapes.size(); s++) {
//     Shape shape;
//     shape.indexOffset = indices.size();
//     // use first material used in this mesh instead of materials per face
//     shape.materialId = shapes[s].mesh.material_ids[0];
//     // iterate over faces
//     size_t index_offset = 0;
//     for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
//       int fv = shapes[s].mesh.num_face_vertices[f];
//       // int material = shapes[s].mesh.material_ids[f];

//       // iterate over the face's vertices
//       for (size_t v = 0; v < fv; v++) {
//         tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
//         // check if we already have such a vertex
//         auto it = vertexToIndexMap.find(idx);
//         if (it == vertexToIndexMap.end()) {
//           glm::vec3 pos = glm::vec3(attrib.vertices[3 * idx.vertex_index + 0],
//                           attrib.vertices[3 * idx.vertex_index + 1],
//                           attrib.vertices[3 * idx.vertex_index + 2]);
//           glm::vec3 normal;
//           if (idx.normal_index >= 0)
//             normal = glm::vec3(attrib.normals[3 * idx.normal_index + 0],
//                                attrib.normals[3 * idx.normal_index + 1],
//                                attrib.normals[3 * idx.normal_index + 2]);
//           glm::vec2 texcoord;
//           if (idx.texcoord_index >= 0)
//             texcoord =
//                 glm::vec2(attrib.texcoords[2 * idx.texcoord_index + 0],
//                           1.0f - attrib.texcoords[2 * idx.texcoord_index + 1]);
          
//           uint32_t index = vertices.size();
//           vertexToIndexMap.emplace(idx, index);
//           vertices.push_back(Vertex(pos, normal, texcoord, material));
//           indices.push_back(index);
//         } 
//         else {
//           indices.push_back(it->second);
//         }
//       }
//       index_offset += fv;
//     }
//     shape.numIndices = indices.size() - shape.indexOffset;
//     m_shapes.push_back(shape);
//   }

//   auto verticesSize = sizeof(vertices[0]) * vertices.size();
//   auto indicesSize = sizeof(indices[0]) * indices.size();
//   // m_buffer =
//   //     std::make_unique<Buffer>(m_log, m_device, verticesSize + indicesSize,
//   //                              vk::BufferUsageFlagBits::eVertexBuffer |
//   //                                  vk::BufferUsageFlagBits::eIndexBuffer,
//   //                              vk::MemoryPropertyFlagBits::eDeviceLocal);
//   // m_buffer->upload(vertices.data(), verticesSize, 0);
//   // m_buffer->upload(indices.data(), indicesSize, verticesSize);

//   m_numIndices = indices.size();
//   m_indexOffset = verticesSize;
// }
void generate_normals(tinyobj::mesh_t& model) {
  std::vector<glm::fvec3> positions(model.positions.size() / 3);

  for (unsigned i = 0; i < model.positions.size(); i+=3) {
    positions[i / 3] = glm::fvec3{model.positions[i], model.positions[i + 1], model.positions[i + 2]};
  }

  std::vector<glm::fvec3> normals(model.positions.size() / 3, glm::fvec3{0.0f});
  for (unsigned i = 0; i < model.indices.size(); i+=3) {
    glm::fvec3 normal = glm::cross(positions[model.indices[i+1]] - positions[model.indices[i]], positions[model.indices[i+2]] - positions[model.indices[i]]);

    normals[model.indices[i]] += normal;
    normals[model.indices[i+1]] += normal;
    normals[model.indices[i+2]] += normal;
  }

  model.normals.reserve(model.positions.size());
  for (unsigned i = 0; i < normals.size(); ++i) {
    glm::fvec3 normal = glm::normalize(normals[i]);
    model.normals[i * 3] = normal[0];
    model.normals[i * 3 + 1] = normal[1];
    model.normals[i * 3 + 2] = normal[2];
  }
}

std::vector<glm::fvec3> generate_tangents(tinyobj::mesh_t const& model) {
  // containers for vetex attributes
  std::vector<glm::fvec3> positions(model.positions.size() / 3);
  std::vector<glm::fvec3> normals(model.positions.size() / 3);
  std::vector<glm::fvec2> texcoords(model.positions.size() / 3);
  std::vector<glm::fvec3> tangents(model.positions.size() / 3, glm::fvec3{0.0f});

  // get vertex positions and texture coordinates from mesh_t
  for (unsigned i = 0; i < model.positions.size(); i+=3) {
    positions[i / 3] = glm::fvec3{model.positions[i],
                                 model.positions[i + 1],
                                 model.positions[i + 2]};
    normals[i / 3] = glm::fvec3{model.normals[i],
                                 model.normals[i + 1],
                                 model.normals[i + 2]};
  }
  for (unsigned i = 0; i < model.texcoords.size(); i+=2) {
    texcoords[i / 2] = glm::fvec2{model.texcoords[i], model.texcoords[i + 1]};
  }

  // calculate tangent for triangles
  for (unsigned i = 0; i < model.indices.size() / 3; i++) {
    // indices of vertices of this triangle, access any attribute of first vert with "attribute[indices[0]]"
    unsigned indices[3] = {model.indices[i * 3],
                           model.indices[i * 3 + 1],
                           model.indices[i * 3 + 2]};
    
    // implement tangent calculation here
  }

  for (unsigned i = 0; i < tangents.size(); ++i) {
    // implement orthogonalization and normalization here
  }

  throw std::logic_error("Tangent creation not implemented yet");

  return tangents;
}

struct serialized_triangle {
  float v0_x_, v0_y_, v0_z_;   //vertex 0
  float n0_x_, n0_y_, n0_z_;   //normal 0
  float c0_x_, c0_y_;          //texcoord 0
  float v1_x_, v1_y_, v1_z_;
  float n1_x_, n1_y_, n1_z_;
  float c1_x_, c1_y_;
  float v2_x_, v2_y_, v2_z_;
  float n2_x_, n2_y_, n2_z_;
  float c2_x_, c2_y_;
};

model_t bvh(std::string const& path, std::size_t idx_node) {

  vklod::bvh bvh(path);
  std::cout << "bvh has " << bvh.get_num_nodes() << " nodes" << std::endl;
  
  size_t stride_in_bytes = sizeof(serialized_triangle) * bvh.get_primitives_per_node();
  // for (vklod::node_t node_id = 0; node_id < bvh.get_num_nodes(); ++node_id) {
    
  //   //e.g. compute offset to data in lod file for current node
  //   size_t offset_in_bytes = node_id * stride_in_bytes;

  //   std::cout << "node_id " << node_id
  //             << ", depth " << bvh.get_depth_of_node(node_id)
  //             << ", address " << offset_in_bytes 
  //             << ", length " << stride_in_bytes << std::endl;
  // }
  std::string path_lod = path.substr(0, path.size() - 3) + "lod";
  std::vector<float> tri_data(stride_in_bytes / 4, 0);
  lamure::ren::lod_stream access;
  access.open(path_lod);
  access.read((char*)tri_data.data(), stride_in_bytes * idx_node, stride_in_bytes);
  access.close();
  return model_t{tri_data, model_t::POSITION | model_t::NORMAL | model_t::TEXCOORD};
}

vklod::bvh bvh(std::string const& path) {

  return vklod::bvh{path};
}

lamure::ren::lod_stream&& lod(std::string const& path) {
  lamure::ren::lod_stream access;
  access.open(path);
  return std::move(access);
}

};