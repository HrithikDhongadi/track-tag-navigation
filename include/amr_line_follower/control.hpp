#pragma once
#include <algorithm>
#include <array>
#include <cmath>

struct Command { double speed=0, turn=0; bool line=false; };
// Values are the fraction of dark pixels in each camera, left to right.
inline Command steer(const std::array<double,5> &dark, double speed, double kp) {
  double sum=0, weighted=0;
  for (int i=0;i<5;++i) {
    if (dark[i]<0.10) continue; // ignore isolated dark edge pixels
    sum+=dark[i]; weighted+=(2-i)*dark[i];
  }
  if (sum<0.25) return {}; // no line: stop, do not search blindly
  const double error=weighted/sum;
  return {speed/(1.0+0.5*std::abs(error)),
          std::clamp(kp*error,-0.8,0.8),true};
}
