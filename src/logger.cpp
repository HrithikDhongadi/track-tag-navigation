#include "amr/logger.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>

namespace amr {

class Logger::FileHolder {
 public:
  std::map<LogChannel, std::ofstream> streams;
};

namespace {
const char *channelName(LogChannel channel) {
  switch (channel) {
    case LogChannel::Core: return "core";
    case LogChannel::Controller: return "controller";
    case LogChannel::Navigation: return "navigation";
    case LogChannel::Ui: return "ui";
  }
  return "core";
}

std::string sessionName() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm local{};
  localtime_r(&time, &local);
  std::ostringstream name;
  name << "logs/session_" << std::put_time(&local, "%Y%m%d_%H%M%S");
  return name.str();
}
}  // namespace

Logger &Logger::instance() {
  static Logger logger;
  return logger;
}

void Logger::initialize(LogChannel defaultChannel) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (initialized_) return;
  defaultChannel_ = defaultChannel;
  const char *configuredSession = std::getenv("TRACK_TAG_LOG_SESSION");
  sessionPath_ = configuredSession && *configuredSession ? configuredSession : sessionName();
  std::error_code error;
  std::filesystem::create_directories(sessionPath_, error);
  file_ = new FileHolder;
  initialized_ = true;
}

void Logger::write(LogLevel level, const std::string &message) {
  write(defaultChannel_, level, message);
}

void Logger::write(LogChannel channel, LogLevel level, const std::string &message) {
  if (!initialized_) initialize(channel);
  std::lock_guard<std::mutex> lock(mutex_);
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm local{};
  localtime_r(&time, &local);
  const char *label = level == LogLevel::Debug ? "DEBUG" : level == LogLevel::Info ? "INFO" :
                      level == LogLevel::Warning ? "WARN" : "ERROR";
  std::ostringstream line;
  line << '[' << std::put_time(&local, "%F %T") << "] [" << channelName(channel) << "] [" << label << "] " << message;
  std::cout << line.str() << std::endl;
  if (!file_) return;
  auto &stream = file_->streams[channel];
  if (!stream.is_open()) stream.open(sessionPath_ + "/" + channelName(channel) + ".log", std::ios::out | std::ios::app);
  if (stream) { stream << line.str() << '\n'; stream.flush(); }
}

}  // namespace amr
