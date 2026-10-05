#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace arf {

struct Point {
  float x = 0, y = 0, pressure = 1;
};

struct Stroke {
  std::vector<Point> points;
  std::uint32_t color = 0xFF111111;  // ARGB
  float size = 4;
  bool eraser = false;
};

struct Drawing {
  std::vector<Stroke> strokes;
};

struct Layer {
  std::string name;
  bool visible = true;
  bool locked = false;
  float opacity = 1.0f;
  // Quadro (começa em 1) -> desenho. Cada desenho vale até o próximo quadro-chave.
  std::map<int, Drawing> keys;
};

// Camadas livres + desenhos frame a frame + histórico de desfazer/refazer.
class Animation {
public:
  int width = 1280;
  int height = 720;
  int fps = 24;
  int frameCount = 48;

  int addLayer(std::string name);
  Layer* layer(int index);
  const std::vector<Layer>& layers() const { return layers_; }

  const Drawing* drawingAt(int layer, int frame) const;
  bool hasKey(int layer, int frame) const;
  void insertBlankKey(int layer, int frame);
  bool addStroke(int layer, int frame, Stroke stroke);

  bool undo();
  bool redo();
  bool canUndo() const { return !undo_.empty(); }
  bool canRedo() const { return !redo_.empty(); }

private:
  struct Action {
    int layer;
    int frame;
    Stroke stroke;
    bool createdKey;
  };
  static constexpr std::size_t kMaxHistory = 200;

  std::vector<Layer> layers_;
  std::vector<Action> undo_, redo_;
};

}  // namespace arf
