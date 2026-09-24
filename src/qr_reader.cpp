#include "amr/qr_reader.hpp"
#include "amr/runtime.hpp"
#include "amr/mission_executor.hpp"
#include "amr/route_manager.hpp"

#include <ignition/msgs/image.pb.h>
#include <ignition/msgs/stringmsg.pb.h>
#include <ignition/transport/Node.hh>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <zbar.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace amr {
namespace {
using Clock = std::chrono::steady_clock;
}  // namespace

int runQrReader(bool view, bool frontView, std::atomic_bool &running,
                std::shared_ptr<RouteManager> routes, std::shared_ptr<MissionExecutor> mission) {
  cv::setNumThreads(1);
  std::mutex mutex;
  cv::Mat latest;
  cv::Mat latestFront;
  unsigned long sequence = 0, consumed = 0;
  unsigned long frontSequence = 0, frontConsumed = 0;
  Clock::time_point received = Clock::now();
  ignition::transport::Node node;
  auto checkpointPublisher = node.Advertise<ignition::msgs::StringMsg>("/amr/checkpoint");
  std::function<void(const ignition::msgs::Image &)> callback =
      [&](const ignition::msgs::Image &message) {
        const size_t width = message.width(), height = message.height(), stride = message.step();
        if (message.pixel_format_type() != ignition::msgs::RGB_INT8 || !width || !height ||
            width > 4096 || height > 4096 || stride < 3 * width ||
            stride > message.data().size() / height) return;
        cv::Mat rgb(static_cast<int>(height), static_cast<int>(width), CV_8UC3,
                    const_cast<char *>(message.data().data()), stride);
        cv::Mat gray;
        cv::cvtColor(rgb, gray, cv::COLOR_RGB2GRAY);
        std::lock_guard<std::mutex> lock(mutex);
        latest = gray;
        received = Clock::now();
        ++sequence;
      };
  std::function<void(const ignition::msgs::Image &)> frontCallback =
      [&](const ignition::msgs::Image &message) {
        const size_t width = message.width(), height = message.height(), stride = message.step();
        if (message.pixel_format_type() != ignition::msgs::RGB_INT8 || !width || !height ||
            width > 4096 || height > 4096 || stride < 3 * width ||
            stride > message.data().size() / height) return;
        cv::Mat rgb(static_cast<int>(height), static_cast<int>(width), CV_8UC3,
                    const_cast<char *>(message.data().data()), stride);
        std::lock_guard<std::mutex> lock(mutex);
        latestFront = rgb.clone();
        ++frontSequence;
      };
    if (!node.Subscribe<ignition::msgs::Image>("/amr/qr/image", callback)) {
    std::cerr << "Cannot subscribe to QR camera\n";
    return 1;
  }
  if (frontView && !node.Subscribe<ignition::msgs::Image>("/amr/front/image", frontCallback)) {
    std::cerr << "Cannot subscribe to front camera\n";
    return 1;
  }
    zbar::ImageScanner scanner;
  scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 0);
  scanner.set_config(zbar::ZBAR_QRCODE, zbar::ZBAR_CFG_ENABLE, 1);
  std::map<std::string, double> lastSeen;
  std::string lastLocation = "unknown";
  auto status = Clock::now();
  std::cout << "Listening to /amr/qr/image. No motion commands are sent.\n";
  if (view) {
    cv::namedWindow("QR camera", cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO);
    cv::resizeWindow("QR camera", 640, 480);
  }
  if (frontView) {
    cv::namedWindow("Front camera", cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO);
    cv::resizeWindow("Front camera", 640, 480);
  }
  while (isRunning(running)) {
    cv::Mat frame;
    cv::Mat frontFrame;
    Clock::time_point frameReceived;
    {
      std::lock_guard<std::mutex> lock(mutex);
      frameReceived = received;
      if (sequence != consumed) { frame = latest; consumed = sequence; }
      if (frontSequence != frontConsumed) {
        frontFrame = latestFront;
        frontConsumed = frontSequence;
      }
    }
    if (!frame.empty()) {
      const double now = std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
      zbar::Image image(frame.cols, frame.rows, "Y800", frame.data, frame.total());
      scanner.scan(image);
      for (auto symbol = image.symbol_begin(); symbol != image.symbol_end(); ++symbol) {
        const std::string id = symbol->get_data();
        if (id.empty()) continue;
        const auto previous = lastSeen.find(id);
        if (previous == lastSeen.end() || now - previous->second > 3.0)
          std::cout << "Location: " << id << std::endl;
        lastSeen[id] = now;
        lastLocation = id;
        if (routes) routes->onCheckpoint(id);
        if (mission) mission->onCheckpoint(id);
        if (checkpointPublisher) { ignition::msgs::StringMsg event; event.set_data(id); checkpointPublisher.Publish(event); }
      }
      image.set_data(nullptr, 0);
      if (view) {
        cv::Mat display;
        cv::cvtColor(frame, display, cv::COLOR_GRAY2BGR);
        cv::putText(display, "Last checkpoint: " + lastLocation, {8, 22},
                    cv::FONT_HERSHEY_SIMPLEX, .5, {0, 180, 0}, 1);
        cv::imshow("QR camera", display);
      }
    }
    if (frontView && !frontFrame.empty()) cv::imshow("Front camera", frontFrame);
    if (Clock::now() - status > std::chrono::seconds(5)) {
      if (!consumed || Clock::now() - frameReceived > std::chrono::seconds(3))
        std::cout << "Waiting for camera frames: is simulation playing?\n";
      else
        std::cout << "Camera active; last checkpoint: " << lastLocation << std::endl;
      status = Clock::now();
    }
    if (view || frontView) {
      const int key = cv::waitKey(10);
      if (key == 27 || key == 'q') running.store(false, std::memory_order_relaxed);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }
  if (view || frontView) cv::destroyAllWindows();
  return 0;
}

}  // namespace amr
