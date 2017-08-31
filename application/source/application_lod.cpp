#include "app/launcher_win.hpp"

#include "application_lod.hpp"

int main(int argc, char* argv[]) {
  LauncherWin::run<ApplicationLod>(argc, argv);
}
