#include "app/launcher_win.hpp"

#include "application_clustered.hpp"

int main(int argc, char* argv[]) {
  LauncherWin::run<ApplicationClustered>(argc, argv);
}
