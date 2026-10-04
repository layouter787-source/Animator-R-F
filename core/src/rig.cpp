#include "arf/rig.h"

#include <utility>

namespace arf {

int Rig::addBone(std::string name, int parent, double length) {
  Bone b;
  b.id = next_++;
  b.name = std::move(name);
  b.parent = parent;
  b.length = length;
  bones_.push_back(std::move(b));
  return bones_.back().id;
}

Pose Rig::capture() const {
  Pose p;
  for (const auto& b : bones_) p[b.id] = b.rotation;
  return p;
}

void Rig::apply(const Pose& pose) {
  for (auto& b : bones_) {
    auto it = pose.find(b.id);
    if (it != pose.end()) b.rotation = it->second;
  }
}

}  // namespace arf
