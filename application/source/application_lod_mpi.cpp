#include "app/launcher.hpp"

#include "app/launcher_win.hpp"
#include "app/launcher.hpp"

#include "app/application_worker.hpp"
#include "app/application_win.hpp"

#include "app/application_single.hpp"
#include "app/application_threaded.hpp"
#include "app/application_threaded_transfer.hpp"

#include "application_present.hpp"
#include "application_lod.hpp"
// #include "application_simple.hpp"

#include <mpi.h>

int main(int argc, char* argv[]) {
  // MPI::Init (argc, argv);
  int provided;
  // auto thread_type = MPI_THREAD_FUNNELED;
  auto thread_type = MPI_THREAD_SERIALIZED;
  MPI_Init_thread(&argc, &argv, thread_type, &provided);
  bool threads_ok = provided >= thread_type;
  assert(threads_ok);

  MPI::COMM_WORLD.Set_errhandler(MPI::ERRORS_THROW_EXCEPTIONS);
  if (MPI::COMM_WORLD.Get_errhandler() != MPI::ERRORS_THROW_EXCEPTIONS) {
    throw std::runtime_error{"handler wrong"};
  }
  uint32_t num_processes = MPI::COMM_WORLD.Get_size();
  if (!(num_processes == 2 || (num_processes > 2 && num_processes % 2 == 1))) {
    std::cerr << "number of processes must be 2 or a larger poer of two +1" << std::endl;
    throw std::runtime_error{"invalid process number"};
  }

  double time_start = MPI::Wtime();
  std::cout << "Hello World, my rank is " << MPI::COMM_WORLD.Get_rank() << " of " << MPI::COMM_WORLD.Get_size() <<", response time "<< MPI::Wtime() - time_start << std::endl;
  if (MPI::COMM_WORLD.Get_rank() == 0) {
    LauncherWin::run<ApplicationPresent<ApplicationSingle<ApplicationWin>>>(argc, argv);
  }
  else {
    Launcher::run<ApplicationLod<ApplicationThreadedTransfer<ApplicationWorker>>>(argc, argv);
  }

  MPI::Finalize();
}