#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace arf {

struct Point {
  float x = 0, y = 0, pressure = 1;
  float t = 0;  // segundos desde o início do traço (os pincéis MyPaint usam a velocidade)
};

// Tipos de pincel: lápis (traço firme), tinta (pressão + afinamento nas pontas), macio (bordas suaves).
enum class BrushType : std::uint8_t { Pencil, Ink, Soft };

struct Stroke {
  std::vector<Point> points;
  std::uint32_t color = 0xFF111111;  // ARGB
  float size = 4;
  BrushType brush = BrushType::Pencil;
  float hardness = 1.0f;  // 1 = borda dura; menor = mais macio (só no pincel macio)
  float opacity = 1.0f;
  bool antialias = true;  // false = traço pixelado
  bool eraser = false;
  std::string preset;  // vazio = pincéis internos; senão, id de um pincel MyPaint (ex.: "classic/pencil")
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
  int keyFrameAt(int layer, int frame) const;  // quadro-chave que vale neste quadro (0 = nenhum)
  bool hasKey(int layer, int frame) const;
  void insertBlankKey(int layer, int frame);
  bool addStroke(int layer, int frame, Stroke stroke);

  // ---- estrutura da linha do tempo (limpam o histórico de desfazer) ----
  int lastKey(int layer) const;                     // maior quadro-chave da camada (0 = nenhum)
  int nextKeyAfter(int layer, int frame) const;     // próximo quadro-chave depois de `frame` (0 = nenhum)
  int keyDuration(int layer, int key) const;        // quadros que o desenho ocupa (o último vai até o fim)
  bool setKeyDuration(int layer, int key, int duration);
  int addDrawingAfter(int layer, int frame);        // desenho em branco logo depois do atual; devolve o quadro
  bool deleteDrawing(int layer, int key);           // remove e fecha o espaço
  void ensureFrames(int frames);                    // aumenta o total de quadros
  bool removeLayer(int index);                      // mantém ao menos uma camada
  bool moveLayer(int from, int to);

  bool undo();
  bool redo();
  bool canUndo() const { return !undo_.empty(); }
  bool canRedo() const { return !redo_.empty(); }
  void clearHistory() {
    undo_.clear();
    redo_.clear();
  }

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
