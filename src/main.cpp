#include "amr/line_follower.hpp"
#include "amr/options.hpp"
#include "amr/qr_reader.hpp"
#include "amr/runtime.hpp"

#include <atomic>
#include <string>
#include <thread>

int main(int argc, char **argv) {
  amr::Options options;
  if (!amr::parseOptions(argc, argv, options))
    return argc > 1 && std::string(argv[1]) == "--help" ? 0 : 1;

  amr::installSignalHandlers();
  std::atomic_bool running = true;
  int lineStatus = 0, qrStatus = 0;
  std::thread qrThread([&] {
    qrStatus = amr::runQrReader(options.view, running);
    if (qrStatus) running.store(false, std::memory_order_relaxed);
  });
  std::thread lineThread([&] {
    lineStatus = amr::runLineFollower(options, running);
    if (lineStatus) running.store(false, std::memory_order_relaxed);
  });
  qrThread.join();
  lineThread.join();
  return lineStatus || qrStatus ? 1 : 0;
}
