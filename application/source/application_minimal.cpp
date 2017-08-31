#include "app/launcher_win.hpp"

#include "application_minimal.hpp"

int main(int argc, char* argv[]) {
  LauncherWin::run<ApplicationMinimal>(argc, argv);
}
