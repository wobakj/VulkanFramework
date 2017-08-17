#include "application_minimal.hpp"

#include "app/application_single.hpp"
#include "app/application_win.hpp"
#include "app/launcher_win.hpp"

// exe entry point
int main(int argc, char* argv[]) {
  LauncherWin::run<ApplicationMinimal<ApplicationSingle<ApplicationWin>>>(argc, argv);
}