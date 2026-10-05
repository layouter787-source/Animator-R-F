#include <cmath>
#include <cstdio>

#include "arf/drawing.h"
#include "arf/project.h"
#include "arf/rig.h"
#include "arf/timeline.h"

static int failures = 0;
#define CHECK(cond)                                                  \
  do {                                                               \
    if (!(cond)) {                                                   \
      std::printf("FALHOU: %s (linha %d)\n", #cond, __LINE__);      \
      ++failures;                                                    \
    }                                                                \
  } while (0)

static arf::Stroke dot(float x, float y) {
  arf::Stroke s;
  s.points.push_back({x, y, 1.0f});
  return s;
}

int main() {
  arf::Track t;
  t.setKey(0, 0.0);
  t.setKey(10, 10.0);
  CHECK(std::fabs(t.valueAt(5) - 5.0) < 1e-9);
  CHECK(t.valueAt(-3) == 0.0 && t.valueAt(99) == 10.0);
  CHECK(std::fabs(arf::applyEase(arf::Ease::EaseInOut, 0.5) - 0.5) < 1e-9);
  CHECK(t.removeKey(10) && t.keys().size() == 1);

  arf::Project p;
  auto id = p.scenes[0].addNode("Personagem");
  CHECK(p.scenes[0].find(id) != nullptr);
  CHECK(p.scenes[0].find(999) == nullptr);

  arf::Rig rig;
  int hip = rig.addBone("hip", -1, 2.0);
  arf::Pose a = rig.capture();
  a[hip] = 30.0;
  rig.apply(a);
  CHECK(rig.capture().at(hip) == 30.0);

  // Desenho: quadros-chave com "hold", desfazer e refazer.
  arf::Animation anim;
  int l0 = anim.addLayer("Camada 1");
  CHECK(anim.drawingAt(l0, 1) == nullptr);
  CHECK(anim.addStroke(l0, 3, dot(1, 1)));
  CHECK(anim.hasKey(l0, 3) && !anim.hasKey(l0, 4));
  CHECK(anim.drawingAt(l0, 2) == nullptr);                    // antes da chave
  CHECK(anim.drawingAt(l0, 5) == anim.drawingAt(l0, 3));      // hold
  CHECK(anim.keyFrameAt(l0, 2) == 0 && anim.keyFrameAt(l0, 5) == 3);
  CHECK(anim.addStroke(l0, 3, dot(2, 2)));
  CHECK(anim.drawingAt(l0, 3)->strokes.size() == 2);
  anim.insertBlankKey(l0, 6);
  CHECK(anim.drawingAt(l0, 7)->strokes.empty());              // chave em branco corta o hold
  CHECK(anim.keyFrameAt(l0, 7) == 6);
  CHECK(anim.undo());
  CHECK(anim.drawingAt(l0, 3)->strokes.size() == 1);
  CHECK(anim.redo());
  CHECK(anim.drawingAt(l0, 3)->strokes.size() == 2);
  CHECK(anim.undo() && anim.undo());
  CHECK(!anim.hasKey(l0, 3));                                 // chave criada pelo traço some
  CHECK(!anim.canUndo() && anim.canRedo());
  anim.layer(l0)->locked = true;
  CHECK(!anim.addStroke(l0, 1, dot(0, 0)));
  CHECK(!anim.addStroke(l0, 999, dot(0, 0)));

  // Valores padrão do traço: suave (anti-serrilhado ligado) e opaco.
  arf::Stroke d = dot(0, 0);
  CHECK(d.antialias && d.opacity == 1.0f && d.hardness == 1.0f);

  if (failures == 0) std::puts("OK");
  return failures == 0 ? 0 : 1;
}
