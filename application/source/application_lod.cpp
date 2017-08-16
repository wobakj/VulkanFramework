#include "app/launcher_win.hpp"

#include "app/application_win.hpp"
#include "app/application_threaded_transfer.hpp"

#include "application_lod.hpp"

int main(int argc, char* argv[]) {
  LauncherWin::run<ApplicationLod<ApplicationThreadedTransfer<ApplicationWin>>>(argc, argv);
}