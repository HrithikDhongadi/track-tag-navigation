#pragma once

#include <mutex>
#include <sstream>
#include <string>
#include <utility>

namespace amr {

enum class LogLevel { Debug, Info, Warning, Error };
enum class LogChannel { Core, Controller, Navigation, Ui };

// Process-wide, thread-safe logger. It mirrors events to the terminal and an
// append-only file so headless runs remain diagnosable after completion.
class Logger {
 public:
  static Logger &instance();
  void initialize(LogChannel defaultChannel = LogChannel::Core);
  void write(LogLevel level, const std::string &message);
  void write(LogChannel channel, LogLevel level, const std::string &message);

  template <typename... Values>
  void info(Values &&...values) { log(LogLevel::Info, std::forward<Values>(values)...); }
  template <typename... Values>
  void warning(Values &&...values) { log(LogLevel::Warning, std::forward<Values>(values)...); }
  template <typename... Values>
  void error(Values &&...values) { log(LogLevel::Error, std::forward<Values>(values)...); }
  template <typename... Values>
  void navigation(Values &&...values) { log(LogChannel::Navigation, LogLevel::Info, std::forward<Values>(values)...); }
  template <typename... Values>
  void controller(Values &&...values) { log(LogChannel::Controller, LogLevel::Info, std::forward<Values>(values)...); }
  template <typename... Values>
  void ui(Values &&...values) { log(LogChannel::Ui, LogLevel::Info, std::forward<Values>(values)...); }
  template <typename... Values>
  void core(Values &&...values) { log(LogChannel::Core, LogLevel::Info, std::forward<Values>(values)...); }

 private:
  template <typename... Values>
  void log(LogLevel level, Values &&...values) {
    std::ostringstream stream;
    (stream << ... << std::forward<Values>(values));
    write(level, stream.str());
  }
  template <typename... Values>
  void log(LogChannel channel, LogLevel level, Values &&...values) {
    std::ostringstream stream;
    (stream << ... << std::forward<Values>(values));
    write(channel, level, stream.str());
  }

  Logger() = default;
  std::mutex mutex_;
  std::string sessionPath_;
  LogChannel defaultChannel_ = LogChannel::Core;
  bool initialized_ = false;
  class FileHolder;
  FileHolder *file_ = nullptr;
};

}  // namespace amr
