#ifndef LAUNCHER_HPP
#define LAUNCHER_HPP

#include "wrap/instance.hpp"
#include "wrap/device.hpp"

#include "app/application_threaded_transfer.hpp"
#include "app/application_threaded.hpp"
#include "app/application_single.hpp"
#include "app/application_worker.hpp"

#include "cmdline.h"

#include <string>
#include <memory>

// forward declarations
class Application;

class Launcher {
 public:
  template<template<typename> class T>
  static void run(int argc, char* argv[]) {
    // get parser without specific arguments
    auto cmd_parse = getParser<ApplicationWorker>();
    cmd_parse.parse(argc, argv);

    Launcher launcher{argv[0], cmd_parse.exist("threads")};

    if (cmd_parse.exist("threads")) {
      int threads = cmd_parse.get<int>("threads");
      if (threads == 1) {
        auto cmd_parse = getParser<T<ApplicationSingle<ApplicationWorker>>>();
        cmd_parse.parse_check(argc, argv);
        launcher.runApp<T<ApplicationSingle<ApplicationWorker>>>(cmd_parse);
      }
      else if (threads == 2) {
        auto cmd_parse = getParser<T<ApplicationThreaded<ApplicationWorker>>>();
        cmd_parse.parse_check(argc, argv);
        launcher.runApp<T<ApplicationThreaded<ApplicationWorker>>>(cmd_parse);
      }
    }
    // no value or 3
    // must use new cmd_parse object, assignment operator fails
    auto cmd_parse2 = getParser<T<ApplicationThreadedTransfer<ApplicationWorker>>>();
    cmd_parse2.parse_check(argc, argv);
    launcher.runApp<T<ApplicationThreadedTransfer<ApplicationWorker>>>(cmd_parse2);
  }

 protected:
  Launcher(std::string const& path_exe, bool debug);
  // run application
  template<typename T>
  void runApp(cmdline::parser const& cmd_parse){
    m_application = std::unique_ptr<ApplicationWorker>{new T{m_resource_path, m_device, {}, cmd_parse}};
    mainLoop();
  };
  
  template<typename T>
  static cmdline::parser getParser() {
    cmdline::parser cmd_parse{T::getParser()};
    cmd_parse.add<int>("threads", 't', "number of threads, default 3", false, 3, cmdline::range(1, 3));
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

  std::unique_ptr<ApplicationWorker> m_application;

  Instance m_instance;
  Device m_device;

  const std::vector<const char*> m_validation_layers;
};
#endif