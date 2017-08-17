#include "app/launcher_win.hpp"

#include "app/application_win.hpp"
#include "app/application_threaded_transfer.hpp"
#include "app/application_threaded.hpp"
#include "app/application_single.hpp"

#include "application_lod.hpp"

int main(int argc, char* argv[]) {
	for(int i = 1; i < argc; ++i) {
    std::string arg{argv[i]};
    if (arg == "-t") {
      std::string arg1{argv[(i + 1) % argc]};
      if (arg1 == "1") {
        LauncherWin::run<ApplicationLod<ApplicationSingle<ApplicationWin>>>(argc, argv);
        return 0;
      }
      else if (arg1 == "2") {
        LauncherWin::run<ApplicationLod<ApplicationThreaded<ApplicationWin>>>(argc, argv);
        return 0;
      }
    }
  }
  LauncherWin::run<ApplicationLod<ApplicationThreadedTransfer<ApplicationWin>>>(argc, argv);
}