#pragma once
#include <map>
#include <string>
#include <vector>

namespace arf {

struct Bone {
  int id = 0;
  std::string name;
  int parent = -1;
  double length = 1.0;
  double rotation = 0.0;  // graus, relativo ao pai
};

// Pose = rotação de cada bone (boneId -> graus).
using Pose = std::map<int, double>;

class Rig {
public:
  int addBone(std::string name, int parent, double length);
  Pose capture() const;
  void apply(const Pose& pose);
  const std::vector<Bone>& bones() const { return bones_; }

private:
  std::vector<Bone> bones_;
  int next_ = 1;
};

}  // namespace arf
