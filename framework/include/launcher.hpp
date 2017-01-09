#ifndef LAUNCHER_HPP
#define LAUNCHER_HPP

#include "device.hpp"
#include "instance.hpp"
#include "swap_chain.hpp"

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
    Launcher launcher{args};
    launcher.runApp<T>(args);
  }

 private:

  Launcher(std::vector<std::string> const& args);
  // run application
  template<typename T>
  void runApp(std::vector<std::string> const& args){
    initialize();

    m_application = new T{m_resource_path, m_device, m_swap_chain, m_window, args};

    mainLoop();
  }

  
  // create window and set callbacks
  void initialize();
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