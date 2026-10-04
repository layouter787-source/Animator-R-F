#include "arf/timeline.h"

#include <algorithm>

namespace arf {

double applyEase(Ease e, double t) {
  t = std::clamp(t, 0.0, 1.0);
  switch (e) {
    case Ease::EaseIn: return t * t;
    case Ease::EaseOut: return t * (2.0 - t);
    case Ease::EaseInOut: return t < 0.5 ? 2.0 * t * t : -1.0 + (4.0 - 2.0 * t) * t;
    default: return t;
  }
}

void Track::setKey(int frame, double value, Ease ease) {
  auto it = std::lower_bound(keys_.begin(), keys_.end(), frame,
                             [](const Keyframe& k, int f) { return k.frame < f; });
  if (it != keys_.end() && it->frame == frame) {
    it->value = value;
    it->ease = ease;
  } else {
    keys_.insert(it, Keyframe{frame, value, ease});
  }
}

bool Track::removeKey(int frame) {
  auto it = std::find_if(keys_.begin(), keys_.end(),
                         [frame](const Keyframe& k) { return k.frame == frame; });
  if (it == keys_.end()) return false;
  keys_.erase(it);
  return true;
}

double Track::valueAt(double frame) const {
  if (keys_.empty()) return 0.0;
  if (frame <= keys_.front().frame) return keys_.front().value;
  if (frame >= keys_.back().frame) return keys_.back().value;
  auto hi = std::upper_bound(keys_.begin(), keys_.end(), frame,
                             [](double f, const Keyframe& k) { return f < k.frame; });
  auto lo = hi - 1;
  double t = (frame - lo->frame) / double(hi->frame - lo->frame);
  return lo->value + (hi->value - lo->value) * applyEase(lo->ease, t);
}

}  // namespace arf
