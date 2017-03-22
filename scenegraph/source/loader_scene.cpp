#include "loader_scene.hpp"

#include "scenegraph.hpp"
#include "node/node.hpp"
#include "light.hpp"

#include <vulkan/vulkan.hpp>
#include <json/json.h>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

#include <fstream>
#include <memory>
#include <iostream>

namespace scene_loader {

void parseVal(Json::Value const& val, Scenegraph* graph, Node* node, std::string const& resource_path);

void parseChildren(Json::Value const& val, Scenegraph* graph, Node* node, std::string const& resource_path) {
  auto children = val["children"];
  if (children.isArray()) {
    for (Json::ArrayIndex i = 0; i < val.size(); ++i) {
      parseVal(children[i], graph, node, resource_path);
    }
  }
}

Node* parseShape(Json::Value const& val, Scenegraph* graph, Node* node_parent, std::string const& resource_path) {
  auto name = val["$DEF"].asString();
  auto path = val["geometry"].asString();
  auto node = graph->createGeometryNode(name, resource_path + path);
  // parse and attach children
  parseChildren(val, graph, node.get(), resource_path);
  // attach to tree
  node_parent->addChild(std::move(node));
  std::cout << "attaching geometry " << name << " to " << node_parent->getName() << std::endl;
  return node_parent->getChild(name);
}

Node* parseLight(Json::Value const& val, Scenegraph* graph, Node* node_parent, std::string const& resource_path) {
  auto name = val["$DEF"].asString();
  auto val_color = val["color"];
  auto val_pos = val["location"];

  light_t light{};
  light.color = glm::fvec3{val_color[0].asFloat(), val_color[1].asFloat(), val_color[2].asFloat()};
  light.position = glm::fvec3{val_pos[0].asFloat(), val_pos[1].asFloat(), val_pos[2].asFloat()};
  light.radius = val["radius"].asFloat();
  light.intensity = val["intensity"].asFloat();
    
  auto node = graph->createLightNode(name, light);
  node->setLocal(glm::translate(glm::fmat4{1.0f}, light.position));
  // parse and attach children
  parseChildren(val, graph, node.get(), resource_path);
  // attach to tree
  node_parent->addChild(std::move(node));
  std::cout << "attaching light " << name << " to " << node_parent->getName() << std::endl;
  return node_parent->getChild(name);
}

Node* parseTransform(Json::Value const& val, Scenegraph* graph, Node* node_parent, std::string const& resource_path) {
  auto name = val["$DEF"].asString();
  auto val_scale = val["scale"];
  auto val_rot = val["rotation"];
  auto val_trans = val["translation"];
  glm::fvec3 scale{1.0f};
  if (!val_scale.isNull()) {
    scale = glm::fvec3{val_scale[0].asFloat(), val_scale[1].asFloat(), val_scale[2].asFloat()};
  }
  glm::fvec3 rot_axis{0.0f, 1.0f, 0.0f};
  float rot_angle = 0.0f;
  if (!val_rot.isNull()) {
    rot_axis = glm::fvec3{val_rot[0].asFloat(), val_rot[1].asFloat(), val_rot[2].asFloat()};
    rot_angle = val_rot[3].asFloat();
  }
  glm::fvec3 translation{0.0f};
  if (!val_trans.isNull()) {
    translation = glm::fvec3{val_trans[0].asFloat(), val_trans[1].asFloat(), val_trans[2].asFloat()};
  }
  // calculate transformation
  glm::fmat4 transform{};
  transform = glm::scale(glm::fmat4{1.0f}, scale) * transform;
  transform = glm::rotate(glm::fmat4{1.0f}, rot_angle, rot_axis) * transform;
  transform = glm::translate(glm::fmat4{1.0f}, translation) * transform;
  // create node
  std::unique_ptr<Node> node{new Node{name, transform}};
  // parse and attach children
  parseChildren(val, graph, node.get(), resource_path);
  // just one node -> merge
  if (node->getChildren().size() == 1) {
    // get child owner ptr
    auto name_child = node->getChildren()[0]->getName();
    auto child = node->detachChild(name_child);
    // apply transform
    child->setLocal(transform * child->getLocal());
    // attach to parent
    node_parent->addChild(std::move(child));
    return node_parent->getChild(name_child);
  }
  else {
    // attach to tree
    node_parent->addChild(std::move(node));
    std::cout << "attaching transform " << name << " to " << node_parent->getName() << std::endl;
    return node_parent->getChild(name);
  }
}

void parseGroup(Json::Value const& val, Scenegraph* graph, Node* node_parent, std::string const& resource_path) {
  // ignore group
  // parse and attach children
  parseChildren(val, graph, node_parent, resource_path);
}

void printVal(const Json::Value &val);

void parseVal(Json::Value const& val, Scenegraph* graph, Node* node_parent, std::string const& resource_path) {
  // std:;cout << val.type() << " ";
  if (val.isObject()) {
    auto name = val["$DEF"];
    if (name.isNull()) {
      throw std::runtime_error{"missing object name"};
    }
    auto type = val["$"];
    if (type.isNull()) {
      throw std::runtime_error{"unknown object type"};
    }

    else if (type == "Shape") {
      parseShape(val, graph, node_parent, resource_path);
    }
    else if (type == "PointLight") {
      parseLight(val, graph, node_parent, resource_path);
    }
    else if (type == "Transform") {
      parseTransform(val, graph, node_parent, resource_path);
    }
    else if (type == "Group") {
      parseGroup(val, graph, node_parent, resource_path);
    }
    else {
      std::cout << "Node " << name.asString() << " has unsupported type " << type.asString() << std::endl;
      parseChildren(val, graph, node_parent, resource_path);
    }
  }
  else if (val.isArray()) {
    for (Json::ArrayIndex i = 0; i < val.size(); ++i) {
      parseVal(val[i], graph, node_parent, resource_path);
    }
  }
}

void printVal( const Json::Value &val ) {
  if (val.isObject()) {
    std::cout << "object - members {" << std::endl;
    for (auto iter = val.begin(); iter != val.end(); ++iter) {
      std::cout << iter.name() << " : ";
      printVal(*iter);
    }
    std::cout << "}" << std::endl;
  }
  else if (val.isArray()) {
    std::cout << "array - values [" << std::endl;
    for (Json::ArrayIndex i = 0; i < val.size(); ++i) {
      std::cout << i << " : ";
      printVal(val[i]);
    }
    std::cout << "]" << std::endl;
  }
  else if( val.isString() ) {
    printf( "string - %s\n", val.asString().c_str() ); 
  } 
  else if( val.isBool() ) {
    printf( "bool - %d\n", val.asBool() ); 
  } 
  else if( val.isInt() ) {
    printf( "int - %d\n", val.asInt() ); 
  } 
  else if( val.isUInt() ) {
    printf( "uint - %u\n", val.asUInt() ); 
  } 
  else if( val.isDouble() ) {
    printf( "double - %f\n", val.asDouble() ); 
  }
  else {
    printf( "unknown type=[%d] \n", val.type() ); 
  }
}

void json(std::string const& file_path, std::string const& resource_path, Scenegraph* graph) {
  std::ifstream stream{file_path, std::ifstream::binary};
  if (!stream) {
    throw std::runtime_error{"scene_loader: filed to load file '" + file_path + "'"};
  }

  Json::CharReaderBuilder rbuilder;
  rbuilder["collectComments"] = false;
  std::string errs;
  Json::Value root;
  bool success = Json::parseFromStream(rbuilder, stream, &root, &errs);
  if (!success) {
    throw std::runtime_error{"scene_loader: " + errs};
  }

  parseVal(root, graph, graph->getRoot(), resource_path);
}

};