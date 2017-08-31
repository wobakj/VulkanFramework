#include "app/launcher_win.hpp"

#include "application_scenegraph_clustered.hpp"

int main(int argc, char* argv[]) {
  LauncherWin::run<ApplicationScenegraphClustered>(argc, argv);
}
