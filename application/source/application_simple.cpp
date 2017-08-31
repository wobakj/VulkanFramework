#include "app/launcher_win.hpp"

#include "application_simple.hpp"

int main(int argc, char* argv[]) {
  LauncherWin::run<ApplicationSimple>(argc, argv);
}
