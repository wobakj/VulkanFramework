#include "model_loader.hpp"

#include "bvh.h"
#include "lod_stream.h"

#include "tiny_obj_loader.h"

// use floats and med precision operations
#include <glm/gtc/type_precision.hpp>
#include <glm/geometric.hpp>

#include <iostream>

namespace tinyobj {
bool operator<(tinyobj::index_t const& lhs,
                      tinyobj::index_t const& rhs) {
  if (lhs.vertex_index == rhs.vertex_index) {
    if (lhs.normal_index == rhs.normal_index) {
      return lhs.texcoord_index < rhs.texcoord_index;
    } else
      return lhs.normal_index < rhs.normal_index;
  } else
    return lhs.vertex_index < rhs.vertex_index;
}

bool operator==(tinyobj::index_t const& lhs,
                      tinyobj::index_t const& rhs) {
  if (lhs.vertex_index == rhs.vertex_index) {
    if (lhs.normal_index == rhs.normal_index) {
      return lhs.texcoord_index == rhs.texcoord_index;
    } else
      return false;
  } else
    return false;
}
};

namespace model_loader {

// void generate_normals(tinyobj::mesh_t& model);
void generate_normals(tinyobj::attrib_t& attribs, tinyobj::mesh_t& mesh);

// std::vector<glm::fvec3> generate_tangents(tinyobj::mesh_t const& vertex_data);

vertex_data obj(std::string const& file_path, vertex_data::attrib_flag_t import_attribs) {
// import
  tinyobj::attrib_t attribs;
  std::vector<tinyobj::shape_t> shapes;
  std::string err;
  std::vector<tinyobj::material_t> m_materials;
  // load obj and triangulate by default
  bool success = tinyobj::LoadObj(&attribs, &shapes, &m_materials, &err, file_path.c_str());
  if (!success) {
    throw std::runtime_error("tinyobjloader: '" + file_path + "' - failed to load");
  }
  else if (!err.empty()) {
    throw std::runtime_error("tinyobjloader: '" + file_path + "' - " + err);
  }
// parameter handling
  vertex_data::attrib_flag_t attributes{vertex_data::POSITION | import_attribs};

  bool has_normals = (import_attribs & vertex_data::NORMAL) != 0;
  if(has_normals) {
    // generate normals if necessary
    if (attribs.normals.empty()) {
      // generate_normals(curr_mesh);
      std::cerr << "Shapes has no normals" << std::endl;
      // attributes ^= vertex_data::NORMAL;
    }
  }

  bool has_uvs = (import_attribs & vertex_data::TEXCOORD) != 0;
  if(has_uvs) {
    if (attribs.texcoords.empty()) {
      has_uvs = false;
      attributes ^= vertex_data::TEXCOORD;
      std::cerr << "Shape has no texcoords" << std::endl;
    }
  }

  bool has_tangents = import_attribs & vertex_data::TANGENT;
  std::vector<glm::fvec3> tangents;
  if (has_tangents) {
    if (!has_uvs) {
      has_tangents = false;
      attributes ^= vertex_data::TANGENT;
      std::cerr << "Shape has no texcoords" << std::endl;
    }
    else {
      throw std::runtime_error("tangent generation not supported");
      // tangents = generate_tangents(curr_mesh);
    }
  }

// data transfer
  std::map<tinyobj::index_t, uint32_t> vertexToIndexMap;
  std::vector<float> vertices{};
  std::vector<uint32_t> indices{};
  
  // iterate over shapes
  uint32_t num_vertices = 0;
  for (auto& shape : shapes) {
    if (has_normals && shape.mesh.indices.front().normal_index < 0) {
      std::cout << "generating normals for shape " << shape.name << std::endl;
      generate_normals(attribs, shape.mesh);
    }
    // use first material used in this mesh instead of materials per face
    // shape.materialId = shape.mesh.material_ids[0];
    // iterate over faces
    size_t offset_face = 0;
    for (auto const& num_face_verts : shape.mesh.num_face_vertices) {
      // get face and material
      // uint8_t num_face_verts = shape.mesh.num_face_vertices[idx_face];
      // int material = shape.mesh.material_ids[idx_face];
      // iterate over the face vertices
      for (uint8_t idx_vert = 0; idx_vert < num_face_verts; ++idx_vert) {
        tinyobj::index_t const& vert_properties = shape.mesh.indices[offset_face + idx_vert];
        // check if we already have such a vertex
        auto it = vertexToIndexMap.find(vert_properties);
        if (it == vertexToIndexMap.end()) {
          vertices.push_back(attribs.vertices[3 * vert_properties.vertex_index + 0]);
          vertices.push_back(attribs.vertices[3 * vert_properties.vertex_index + 1]);
          vertices.push_back(attribs.vertices[3 * vert_properties.vertex_index + 2]);
          
          if (has_normals) { 
            assert(vert_properties.normal_index >= 0);
            vertices.push_back(attribs.normals[3 * vert_properties.normal_index + 0]);
            vertices.push_back(attribs.normals[3 * vert_properties.normal_index + 1]);
            vertices.push_back(attribs.normals[3 * vert_properties.normal_index + 2]);
          }
          if (has_uvs) {
            assert(vert_properties.texcoord_index >= 0);
            vertices.push_back(attribs.texcoords[2 * vert_properties.texcoord_index + 0]);
            vertices.push_back(attribs.texcoords[2 * vert_properties.texcoord_index + 1]);
          }
          uint32_t index = num_vertices;
          vertexToIndexMap.emplace(vert_properties, index);
          indices.push_back(index);
          ++num_vertices;
        } 
        else {
          indices.push_back(it->second);
        }
      }
      offset_face += num_face_verts;
    }
  }
  return vertex_data{vertices, attributes, indices};
}

void generate_normals(tinyobj::attrib_t& attribs, tinyobj::mesh_t& mesh) {
  std::vector<uint32_t> indices;
  std::map<tinyobj::index_t, uint32_t> vertexToIndexMap;
  // save vertex positions as glm vectors
  std::vector<glm::fvec3> positions(mesh.indices.size());
  uint32_t index = 0;
  for (auto const& vert_properties : mesh.indices) {
    auto it = vertexToIndexMap.find(vert_properties);
    if (it == vertexToIndexMap.end()) {
        indices.push_back(index);
    }
    else {
      indices.push_back(it->second);
    }
    positions[index] = glm::fvec3{attribs.vertices[3 * vert_properties.vertex_index + 0]
                             ,attribs.vertices[3 * vert_properties.vertex_index + 1]
                             ,attribs.vertices[3 * vert_properties.vertex_index + 2]
                           };
    ++index;
  }
  // calculate triangle normal 
  std::vector<glm::fvec3> normals(mesh.num_face_vertices.size(), glm::fvec3{0.0f});
  // size_t index_offset = 0;
  for (unsigned i = 0; i < mesh.num_face_vertices.size(); i+=3) {
    assert(mesh.num_face_vertices[i].size() == 3);
    glm::fvec3 normal = glm::cross(positions[i+1] - positions[i], positions[i+2] - positions[i]);
    // accumulate vertex normals
    normals[i] += normal;
    normals[i+1] += normal;
    normals[i+2] += normal;
  }

  for (unsigned i = 0; i < mesh.indices.size(); ++i) {
    tinyobj::index_t& vert_properties = mesh.indices[i];
    // not a duplicate
    auto unique_idx = indices.at(i);
    if (unique_idx == i) {
      vert_properties.normal_index = uint32_t(attribs.normals.size() / 3); 
      glm::fvec3 normal = glm::normalize(normals[i]);
      attribs.normals.emplace_back(normal[0]);
      attribs.normals.emplace_back(normal[1]);
      attribs.normals.emplace_back(normal[2]);
    }
    else {
      vert_properties.normal_index = mesh.indices.at(unique_idx).normal_index; 
    }
  }
}

// std::vector<glm::fvec3> generate_tangents(tinyobj::mesh_t const& model) {
//   // containers for vetex attributes
//   std::vector<glm::fvec3> positions(model.positions.size() / 3);
//   std::vector<glm::fvec3> normals(model.positions.size() / 3);
//   std::vector<glm::fvec2> texcoords(model.positions.size() / 3);
//   std::vector<glm::fvec3> tangents(model.positions.size() / 3, glm::fvec3{0.0f});

//   // get vertex positions and texture coordinates from mesh_t
//   for (unsigned i = 0; i < model.positions.size(); i+=3) {
//     positions[i / 3] = glm::fvec3{model.positions[i],
//                                  model.positions[i + 1],
//                                  model.positions[i + 2]};
//     normals[i / 3] = glm::fvec3{model.normals[i],
//                                  model.normals[i + 1],
//                                  model.normals[i + 2]};
//   }
//   for (unsigned i = 0; i < model.texcoords.size(); i+=2) {
//     texcoords[i / 2] = glm::fvec2{model.texcoords[i], model.texcoords[i + 1]};
//   }

//   // calculate tangent for triangles
//   for (unsigned i = 0; i < model.indices.size() / 3; i++) {
//     // indices of vertices of this triangle, access any attribute of first vert with "attribute[indices[0]]"
//     unsigned indices[3] = {model.indices[i * 3],
//                            model.indices[i * 3 + 1],
//                            model.indices[i * 3 + 2]};
    
//     // implement tangent calculation here
//   }

//   for (unsigned i = 0; i < tangents.size(); ++i) {
//     // implement orthogonalization and normalization here
//   }

//   throw std::logic_error("Tangent creation not implemented yet");

//   return tangents;
// }

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

vertex_data bvh(std::string const& path, std::size_t idx_node) {

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
  return vertex_data{tri_data, vertex_data::POSITION | vertex_data::NORMAL | vertex_data::TEXCOORD};
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