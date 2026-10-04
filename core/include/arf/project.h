#pragma once
#include <map>
#include <string>
#include <vector>

#include "arf/timeline.h"

namespace arf {

using NodeId = int;

struct Transform {
  double x = 0, y = 0, rotation = 0;
  double scaleX = 1, scaleY = 1, opacity = 1;
};

// Como o frame é produzido: desenho, rig, ou desenho manual por cima do rig.
enum class FrameMode { Drawing, Rig, Override };

struct Exposure {
  int startFrame = 1;
  int length = 1;
  int drawingId = -1;  // -1 = quadro em branco
  FrameMode mode = FrameMode::Drawing;
};

// Qualquer elemento (grupo, desenho, rig, câmera...) é um Node com tracks.
struct Node {
  NodeId id = 0;
  std::string name;
  NodeId parent = -1;
  Transform base;
  std::map<std::string, Track> tracks;  // "x", "y", "rotation"...
  std::vector<Exposure> exposures;
};

class Scene {
public:
  int frameCount = 48;

  NodeId addNode(std::string name, NodeId parent = -1);
  Node* find(NodeId id);
  const std::vector<Node>& nodes() const { return nodes_; }

private:
  std::vector<Node> nodes_;
  NodeId next_ = 1;
};

struct Project {
  std::string name = "Sem título";
  int width = 1280;
  int height = 720;
  int fps = 24;
  std::vector<Scene> scenes = std::vector<Scene>(1);
};

}  // namespace arf
