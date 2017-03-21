#ifndef LOADER_SCENE_HPP
#define LOADER_SCENE_HPP

// #include "node/node_camera.hpp"

#include <string>

class Scenegraph;
namespace scene_loader {

void json(std::string const& file_path, Scenegraph* graph);

};

#endif
