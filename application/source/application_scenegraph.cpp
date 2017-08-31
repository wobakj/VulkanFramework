#include "app/launcher_win.hpp"

#include "application_scenegraph.hpp"

int main(int argc, char* argv[]) {
  LauncherWin::run<ApplicationScenegraph>(argc, argv);
}
