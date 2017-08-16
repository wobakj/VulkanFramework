#include "app/launcher_win.hpp"

#include "app/application_win.hpp"
#include "app/application_single.hpp"

#include "application_lod.hpp"

int main(int argc, char* argv[]) {
  LauncherWin::run<ApplicationLod<ApplicationSingle<ApplicationWin>>>(argc, argv);
}