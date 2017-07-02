#ifndef LAUNCHER_WIN_HPP
#define LAUNCHER_WIN_HPP

#include "wrap/device.hpp"
#include "wrap/instance.hpp"
#include "wrap/swap_chain.hpp"
#include "wrap/surface.hpp"

#include "cmdline.h"

#include <string>

// forward declarations
class ApplicationWin;
class GLFWwindow;
// extern SwapChain m_swap_chain;
class LauncherWin {
 public:
  template<typename T>
  static void run(int argc, char* argv[]) {
    std::vector<std::string> args{};
    for (int i = 0; i < argc; ++i) {
      args.emplace_back(argv[i]);
    }
    auto cmd_parse = getParser<T>();
    cmd_parse.parse_check(args);
    LauncherWin launcher{args, cmd_parse};
    launcher.runApp<T>(cmd_parse);
  }

 private:
  LauncherWin(std::vector<std::string> const& args, cmdline::parser const& cmd_parse);
  // run application
  template<typename T>
  void runApp(cmdline::parser const& cmd_parse){
    m_application =  std::unique_ptr<ApplicationWin>{new T{m_resource_path, m_device, m_surface, cmd_parse}};

    mainLoop();
  };
  
  template<typename T>
  static cmdline::parser getParser() {
    auto cmd_parse = T::getParser();
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