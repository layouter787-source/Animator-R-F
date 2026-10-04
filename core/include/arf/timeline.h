#pragma once
#include <vector>

namespace arf {

enum class Ease { Linear, EaseIn, EaseOut, EaseInOut };

struct Keyframe {
  int frame = 0;
  double value = 0.0;
  Ease ease = Ease::Linear;
};

double applyEase(Ease e, double t);

// Sequência de keyframes ordenada por frame.
class Track {
public:
  void setKey(int frame, double value, Ease ease = Ease::Linear);
  bool removeKey(int frame);
  double valueAt(double frame) const;
  const std::vector<Keyframe>& keys() const { return keys_; }

private:
  std::vector<Keyframe> keys_;
};

}  // namespace arf
