#ifndef LAUNCHER_HPP
#define LAUNCHER_HPP

#include "wrap/instance.hpp"
#include "wrap/device.hpp"

#include "cmdline.h"

#include <string>
#include <memory>

// forward declarations
class Application;

class Launcher {
 public:
  template<typename T>
  static void run(int argc, char* argv[]) {
    std::vector<std::string> args{};
    for (int i = 0; i < argc; ++i) {
      args.emplace_back(argv[i]);
    }
    auto cmd_parse = getParser<T>();
    cmd_parse.parse_check(args);
    Launcher launcher{args, cmd_parse};
    launcher.runApp<T>(cmd_parse);
  }

 protected:
  Launcher(std::vector<std::string> const& args, cmdline::parser const& cmd_parse);
  // run application
  template<typename T>
  void runApp(cmdline::parser const& cmd_parse){
    m_application = std::unique_ptr<Application>{new T{m_resource_path, m_device, cmd_parse}};
    mainLoop();
  };
  
  template<typename T>
  static cmdline::parser getParser() {
    auto cmd_parse = T::getParser();
    cmd_parse.add("debug", 'd', "debug with validation layers");
    return cmd_parse;
  }

  void resize(int width, int height);
  // start main loop
  virtual void mainLoop();
  // free resources
  virtual void quit(int status);

  // variables for fps computation
  double m_last_second_time;
  unsigned m_frames_per_second;

  // path to the resource folders
  std::string m_resource_path;

  std::unique_ptr<Application> m_application;

  Instance m_instance;
  Device m_device;

  const std::vector<const char*> m_validation_layers;
};
#endif