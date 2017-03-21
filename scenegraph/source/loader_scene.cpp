#include "loader_scene.hpp"

#include "scenegraph.hpp"

#include <vulkan/vulkan.hpp>
#include <json/json.h>

#include <fstream>
#include <iostream>

namespace scene_loader {

static void parseShape(Json::Value const& val, Scenegraph* graph, Node* node) {
  // auto path = val["geometry"]["&"]
  // node->
}

static void parseLight(Json::Value const& val, Scenegraph* graph, Node* node) {

}

static void parseTransform(Json::Value const& val, Scenegraph* graph, Node* node) {

}

static void parseGroup(Json::Value const& val, Scenegraph* graph, Node* node) {

}

static void printVal(const Json::Value &val);

static void parseVal(Json::Value const& val, Scenegraph* graph, Node* node) {
  // std:;cout << val.type() << " ";
  if (val.isObject()) {
    auto type = val["$"];
    if (type.isNull()) {
      throw std::runtime_error{"unknown objects"};
    }
    else if (type == "Shape") {
      parseShape(val, graph, node);
    }
    else if (type == "Light") {
      parseLight(val, graph, node);
    }
    else if (type == "Transform") {
      parseTransform(val, graph, node);
    }
    else if (type == "Group") {
      parseGroup(val, graph, node);
    }
    std::cout << "object - members {" << std::endl;
    // std::cout << "type " << val["$"] << " ";
    for (auto iter = val.begin(); iter != val.end(); ++iter) {
      std::cout << iter.name() << " : ";
      printVal(*iter);
      // std::cout << element.asString() << std::endl;
     // loadPlugIn( plugins[index].asString() );
    }

    // for (auto const& id : val.getMemberNames()) {
    //     std::cout << id << std::endl;
    // }
    std::cout << "}" << std::endl;
  }
  else if (val.isArray()) {
    std::cout << "array - values [" << std::endl;
    for (Json::ArrayIndex i = 0; i < val.size(); ++i) {
      std::cout << i << " : ";
      printVal(val[i]);
      // std::cout << element.asString() << std::endl;
     // loadPlugIn( plugins[index].asString() );
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

static void printVal( const Json::Value &val ) {
  if (val.isObject()) {
    std::cout << "object - members {" << std::endl;
    // std::cout << "type " << val["$"] << " ";
    for (auto iter = val.begin(); iter != val.end(); ++iter) {
      std::cout << iter.name() << " : ";
      printVal(*iter);
      // std::cout << element.asString() << std::endl;
     // loadPlugIn( plugins[index].asString() );
    }

    // for (auto const& id : val.getMemberNames()) {
    //     std::cout << id << std::endl;
    // }
    std::cout << "}" << std::endl;
  }
  else if (val.isArray()) {
    std::cout << "array - values [" << std::endl;
    for (Json::ArrayIndex i = 0; i < val.size(); ++i) {
      std::cout << i << " : ";
      printVal(val[i]);
      // std::cout << element.asString() << std::endl;
     // loadPlugIn( plugins[index].asString() );
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

  printVal(root);
    // std::cout << element.asString() << std::endl;
   // loadPlugIn( plugins[index].asString() );

  assert(0);
}

};