#include "loader_scene.hpp"

#include "scenegraph.hpp"

#include <vulkan/vulkan.hpp>
#include <json/json.h>

#include <fstream>
#include <iostream>

namespace scene_loader {

void json(std::string const& file_path, Scenegraph* graph) {
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

}

};