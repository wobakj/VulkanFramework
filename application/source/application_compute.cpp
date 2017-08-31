#include "app/launcher_win.hpp"

#include "application_compute.hpp"

int main(int argc, char* argv[]) {
  LauncherWin::run<ApplicationCompute>(argc, argv);
}
