#ifndef LAUNCHER_WIN_HPP
#define LAUNCHER_WIN_HPP

#include "app/launcher.hpp"
// #include "wrap/device.hpp"
// #include "wrap/instance.hpp"
#include "wrap/swap_chain.hpp"
#include "wrap/surface.hpp"

#include "cmdline.h"

#include <string>

// forward declarations
class GLFWwindow;

class LauncherWin : public Launcher {
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
  void runApp(cmdline::parser const& cmd_parse) {
    m_application = new T{m_resource_path, m_device, m_surface, cmd_parse};
    mainLoop();
  };
  

  // start main loop
  void createSurface();
  void mainLoop();
  // update viewport and field of view
  void resize(GLFWwindow* window, int width, int height);
  // handle key input
  void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
  //handle mouse movement input
  // void mouse_callback(GLFWwindow* window, double pos_x, double pos_y);

  // calculate fps and show in window title
  void show_fps();
  // free resources
  void quit(int status);

  // the rendering window
  GLFWwindow* m_window;

  // Device m_device;
  Surface m_surface;
};
#endif