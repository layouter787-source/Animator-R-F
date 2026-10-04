#include <cmath>
#include <cstdio>

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

  if (failures == 0) std::puts("OK");
  return failures == 0 ? 0 : 1;
}
