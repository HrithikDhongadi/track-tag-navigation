#include "amr/json_message.hpp"
#include "amr/logger.hpp"

#include <ignition/msgs/stringmsg.pb.h>
#include <ignition/transport/Node.hh>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <atomic>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace {
constexpr char kDefaultAddress[] = "127.0.0.1";
constexpr unsigned short kDefaultPort = 8080;

struct Options {
  std::filesystem::path mapPath;
  std::filesystem::path webRoot;
  std::string address = kDefaultAddress;
  unsigned short port = kDefaultPort;
};

void usage() {
  std::cout << "track_tag_navigation_web_bridge --map map.json --web-root web/dist "
               "[--address 127.0.0.1] [--port 8080]\n";
}

std::optional<Options> parseOptions(int argc, char **argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--help") { usage(); return std::nullopt; }
    if (argument == "--map" || argument == "--web-root" || argument == "--address" || argument == "--port") {
      if (++i >= argc) { std::cerr << argument << " requires a value\n"; return std::nullopt; }
      const std::string value = argv[i];
      if (argument == "--map") options.mapPath = value;
      else if (argument == "--web-root") options.webRoot = value;
      else if (argument == "--address") options.address = value;
      else {
        try {
          const unsigned long port = std::stoul(value);
          if (port == 0 || port > 65535) throw std::out_of_range("port");
          options.port = static_cast<unsigned short>(port);
        } catch (...) { std::cerr << "--port must be between 1 and 65535\n"; return std::nullopt; }
      }
      continue;
    }
    std::cerr << "Unknown option: " << argument << '\n';
    return std::nullopt;
  }
  if (options.mapPath.empty() || options.webRoot.empty()) { usage(); return std::nullopt; }
  if (options.address != kDefaultAddress) {
    std::cerr << "For safety, the bridge may bind only to " << kDefaultAddress << ".\n";
    return std::nullopt;
  }
  return options;
}

std::optional<std::string> readText(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return std::nullopt;
  return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

std::string contentType(const std::filesystem::path &path) {
  const auto extension = path.extension().string();
  if (extension == ".html") return "text/html; charset=utf-8";
  if (extension == ".js") return "text/javascript; charset=utf-8";
  if (extension == ".css") return "text/css; charset=utf-8";
  if (extension == ".json") return "application/json; charset=utf-8";
  if (extension == ".svg") return "image/svg+xml";
  if (extension == ".png") return "image/png";
  if (extension == ".ico") return "image/x-icon";
  return "application/octet-stream";
}

bool validMissionCommand(const std::string &command) {
  static const std::string allowed[] = {"start", "pause", "resume", "stop", "cancel", "retry", "next"};
  for (const auto &value : allowed) if (command == value) return true;
  if (command.rfind("create:", 0) != 0 || command.size() <= 7) return false;
  for (const unsigned char character : command.substr(7))
    if (!(std::isalnum(character) || character == ' ' || character == '_' || character == '-' || character == ',')) return false;
  return true;
}

class Bridge {
 public:
  explicit Bridge(std::string map) : map_(std::move(map)) {
    telemetryNode_.Subscribe<ignition::msgs::StringMsg>("/amr/telemetry", [this](const auto &message) {
      std::lock_guard<std::mutex> lock(mutex_); telemetry_ = message.data();
    });
    checkpointNode_.Subscribe<ignition::msgs::StringMsg>("/amr/checkpoint", [this](const auto &message) {
      std::lock_guard<std::mutex> lock(mutex_); checkpoint_ = message.data();
    });
    missionNode_.Subscribe<ignition::msgs::StringMsg>("/mission/status", [this](const auto &message) {
      std::lock_guard<std::mutex> lock(mutex_); mission_ = message.data();
    });
    commandPublisher_ = missionNode_.Advertise<ignition::msgs::StringMsg>("/mission/command");
  }

  std::string snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return "{\"schema_version\":1,\"robot_id\":\"amr_1\",\"last_checkpoint\":\"" + amr::escapeJson(checkpoint_) +
      "\",\"telemetry\":" + jsonOrNull(telemetry_) + ",\"mission\":" + jsonOrNull(mission_) + "}";
  }
  const std::string &map() const { return map_; }
  bool command(const std::string &command) {
    if (!validMissionCommand(command) || !commandPublisher_) return false;
    ignition::msgs::StringMsg message;
    message.set_data(command);
    commandPublisher_.Publish(message);
    amr::Logger::instance().ui("web mission command requested: ", command);
    return true;
  }

 private:
  static std::string jsonOrNull(const std::string &value) {
    if (value.size() >= 2 && value.front() == '{' && value.back() == '}') return value;
    return "null";
  }
  mutable std::mutex mutex_;
  std::string map_, telemetry_, checkpoint_, mission_;
  ignition::transport::Node telemetryNode_, checkpointNode_, missionNode_;
  ignition::transport::Node::Publisher commandPublisher_;
};

template <typename Body>
void addHeaders(http::response<Body> &response) {
  response.set(http::field::server, "track-tag-navigation-web-bridge");
  response.set(http::field::access_control_allow_origin, "http://127.0.0.1:8080");
  response.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
  response.set(http::field::access_control_allow_headers, "Content-Type");
}

http::response<http::string_body> jsonResponse(http::status status, std::string body) {
  http::response<http::string_body> response{status, 11};
  addHeaders(response);
  response.set(http::field::content_type, "application/json; charset=utf-8");
  response.body() = std::move(body);
  response.prepare_payload();
  return response;
}

http::response<http::string_body> handleRequest(const http::request<http::string_body> &request,
                                                Bridge &bridge,
                                                const std::filesystem::path &webRoot) {
  if (request.method() == http::verb::options) return jsonResponse(http::status::no_content, "");
  const std::string target = std::string(request.target());
  if (request.method() == http::verb::get && target == "/api/v1/health") return jsonResponse(http::status::ok, "{\"ok\":true}");
  if (request.method() == http::verb::get && target == "/api/v1/status") return jsonResponse(http::status::ok, bridge.snapshot());
  if (request.method() == http::verb::get && target == "/api/v1/map") return jsonResponse(http::status::ok, bridge.map());
  if (request.method() == http::verb::post && target == "/api/v1/mission-commands") {
    const auto command = amr::jsonStringField(request.body(), "command");
    if (!command || !bridge.command(*command)) return jsonResponse(http::status::bad_request, "{\"error\":\"invalid or unavailable mission command\"}");
    return jsonResponse(http::status::accepted, "{\"accepted\":true}");
  }
  if (request.method() != http::verb::get && request.method() != http::verb::head)
    return jsonResponse(http::status::method_not_allowed, "{\"error\":\"method not allowed\"}");
  if (target.find("..") != std::string::npos) return jsonResponse(http::status::forbidden, "{\"error\":\"forbidden\"}");
  const std::string relative = target == "/" ? "index.html" : target.substr(1);
  auto path = webRoot / relative;
  if (!std::filesystem::is_regular_file(path)) path = webRoot / "index.html";
  const auto body = readText(path);
  if (!body) return jsonResponse(http::status::not_found, "{\"error\":\"dashboard build missing\"}");
  http::response<http::string_body> response{http::status::ok, request.version()};
  addHeaders(response);
  response.set(http::field::content_type, contentType(path));
  if (request.method() != http::verb::head) response.body() = *body;
  response.prepare_payload();
  return response;
}

void session(tcp::socket socket, Bridge &bridge, const std::filesystem::path &webRoot) {
  beast::error_code error;
  beast::flat_buffer buffer;
  http::request<http::string_body> request;
  http::read(socket, buffer, request, error);
  if (error) return;
  if (websocket::is_upgrade(request) && request.target() == "/api/v1/stream") {
    websocket::stream<tcp::socket> stream(std::move(socket));
    stream.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
    stream.accept(request, error);
    if (error) return;
    while (true) {
      stream.text(true);
      stream.write(net::buffer(bridge.snapshot()), error);
      if (error) return;
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
  }
  auto response = handleRequest(request, bridge, webRoot);
  http::write(socket, response, error);
  socket.shutdown(tcp::socket::shutdown_send, error);
}

}  // namespace

int main(int argc, char **argv) {
  const auto options = parseOptions(argc, argv);
  if (!options) return 2;
  const auto map = readText(options->mapPath);
  if (!map) { std::cerr << "Cannot read map: " << options->mapPath << '\n'; return 1; }
  if (!std::filesystem::is_regular_file(options->webRoot / "index.html")) {
    std::cerr << "Cannot find dashboard build: " << options->webRoot / "index.html" << '\n'; return 1;
  }
  try {
    net::io_context context{1};
    tcp::acceptor acceptor(context, {net::ip::make_address(options->address), options->port});
    Bridge bridge(*map);
    amr::Logger::instance().ui("web bridge listening at http://", options->address, ":", options->port);
    while (true) {
      tcp::socket socket(context);
      acceptor.accept(socket);
      std::thread(session, std::move(socket), std::ref(bridge), std::cref(options->webRoot)).detach();
    }
  } catch (const std::exception &error) {
    amr::Logger::instance().error("web bridge failed: ", error.what());
    return 1;
  }
}
