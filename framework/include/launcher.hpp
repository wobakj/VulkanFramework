#ifndef LAUNCHER_HPP
#define LAUNCHER_HPP

#include "wrap/device.hpp"
#include "wrap/instance.hpp"
#include "wrap/swap_chain.hpp"

#include "cmdline.h"

#include <string>

// forward declarations
class Application;
class GLFWwindow;

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

 private:
  Launcher(std::vector<std::string> const& args, cmdline::parser const& cmd_parse);
  // run application
  template<typename T>
  void runApp(cmdline::parser const& cmd_parse){
    vk::PresentModeKHR present_mode{};
    std::string mode = cmd_parse.get<std::string>("present");
    if (mode == "fifo") {
      present_mode = vk::PresentModeKHR::eFifo;
    }
    else if (mode == "mailbox") {
      present_mode = vk::PresentModeKHR::eMailbox;
    }
    else if (mode == "immediate") {
      present_mode = vk::PresentModeKHR::eImmediate;
    }
    m_swap_chain.create(m_device, vk::SurfaceKHR{m_surface}, vk::Extent2D{m_window_width, m_window_height}, present_mode, T::imageCount);

    m_application = new T{m_resource_path, m_device, m_swap_chain, m_window, cmd_parse};
    mainLoop();
  };
  
  template<typename T>
  static cmdline::parser getParser() {
    auto cmd_parse = T::getParser();
    cmd_parse.add("debug", 'd', "debug with validation layers");
    cmd_parse.add("present", 'p', "present mode", false, std::string{"fifo"}, cmdline::oneof<std::string>("fifo", "mailbox", "immediate"));
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
  // void mouse_callback(GLFWwindow* window, double pos_x, double pos_y);

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

  Application* m_application;

  Instance m_instance;
  Device m_device;
  SwapChain m_swap_chain;
  Deleter<VkSurfaceKHR> m_surface;

  const std::vector<const char*> m_validation_layers;
};
#endif