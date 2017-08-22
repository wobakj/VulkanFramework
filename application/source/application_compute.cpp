#include "app/launcher_win.hpp"

#include "app/application_win.hpp"
#include "app/application_threaded_transfer.hpp"
#include "app/application_threaded.hpp"
#include "app/application_single.hpp"

#include "application_compute.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
  for(int i = 1; i < argc; ++i) {
    std::string arg{argv[i]};
    if (arg == "-t") {
      std::string arg1{argv[(i + 1) % argc]};
      if (arg1 == "1") {
        LauncherWin::run<ApplicationCompute<ApplicationSingle<ApplicationWin>>>(argc, argv);
        return 0;
      }
      else if (arg1 == "2") {
        LauncherWin::run<ApplicationCompute<ApplicationThreaded<ApplicationWin>>>(argc, argv);
        return 0;
      }
    }
  }
  LauncherWin::run<ApplicationCompute<ApplicationThreadedTransfer<ApplicationWin>>>(argc, argv);
}