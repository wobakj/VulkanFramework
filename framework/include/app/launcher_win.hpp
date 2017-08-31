#ifndef LAUNCHER_WIN_HPP
#define LAUNCHER_WIN_HPP

#include "wrap/device.hpp"
#include "wrap/instance.hpp"
#include "wrap/swap_chain.hpp"
#include "wrap/surface.hpp"

#include "app/application_threaded_transfer.hpp"
#include "app/application_threaded.hpp"
#include "app/application_single.hpp"
#include "app/application_win.hpp"

#include "cmdline.h"

#include <string>

// forward declarations
class ApplicationWin;
class GLFWwindow;
// extern SwapChain m_swap_chain;
class LauncherWin {
 public:
  template<template<typename> class T>
  static void run(int argc, char* argv[]) {
    // get parser without specific arguments
    auto cmd_parse = getParser<ApplicationWin>();
    cmd_parse.parse(argc, argv);

    LauncherWin launcher{argv[0], cmd_parse.exist("threads")};

    if (cmd_parse.exist("threads")) {
      int threads = cmd_parse.get<int>("threads");
      if (threads == 1) {
        auto cmd_parse = getParser<T<ApplicationSingle<ApplicationWin>>>();
        cmd_parse.parse_check(argc, argv);
        launcher.runApp<T<ApplicationSingle<ApplicationWin>>>(cmd_parse);
      }
      else if (threads == 2) {
        auto cmd_parse = getParser<T<ApplicationThreaded<ApplicationWin>>>();
        cmd_parse.parse_check(argc, argv);
        launcher.runApp<T<ApplicationThreaded<ApplicationWin>>>(cmd_parse);
      }
    }
    // no value or 3
    // must use new cmd_parse object, assignment operator fails
    auto cmd_parse2 = getParser<T<ApplicationThreadedTransfer<ApplicationWin>>>();
    cmd_parse2.parse_check(argc, argv);
    launcher.runApp<T<ApplicationThreadedTransfer<ApplicationWin>>>(cmd_parse2);
  }

 private:
  LauncherWin(std::string const& path_exe, bool debug);
  // run application
  template<typename T>
  void runApp(cmdline::parser const& cmd_parse){
    m_application = std::unique_ptr<ApplicationWin>{new T{m_resource_path, m_device, m_surface, cmd_parse}};
    mainLoop();
  };
  
  template<typename T>
  static cmdline::parser getParser() {
    cmdline::parser cmd_parse{T::getParser()};
    cmd_parse.add<int>("threads", 't', "number of threads, default 3", false, 3, cmdline::range(1, 3));
    cmd_parse.add("debug", 'd', "debug with validation layers");
    return cmd_parse;
  }

  // start main loop
  void createSurface();
  void mainLoop();
  // update viewport and field of view
  void resize(GLFWwindow* window, int width, int height);
  // handle key input
  void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
  //handle mouse movement input
  void mouse_callback(GLFWwindow* window, double pos_x, double pos_y);
  void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

  // calculate fps and show in window title
  void show_fps();
  // free resources
  void quit(int status);

  // vertical field of view of camera
  const float m_camera_fov;

  // initial window dimensions
  const unsigned m_window_width;
  const unsigned m_window_height;
  // the rendering window
  GLFWwindow* m_window;

  // variables for fps computation
  double m_last_second_time;
  unsigned m_frames_per_second;

  // path to the resource folders
  std::string m_resource_path;

  std::unique_ptr<ApplicationWin> m_application;

  Instance m_instance;
  Device m_device;
  Surface m_surface;

  const std::vector<const char*> m_validation_layers;
};
#endif