# Animator-R-F

App de animação 2D para **Android (celular primeiro, tablet também)**. Frame a frame, rig e híbrido num único sistema. O rig é sempre opcional e não há camadas ou personagens pré-definidos.

> Desenhe qualquer personagem e anime do seu jeito: frame a frame, com rig ou os dois juntos.

## Stack

- **core/**: C++20 puro (sem Qt): projeto, timeline, rig. Testável sem UI.
- **app/**: Qt 6 + QML (Qt Quick), layout responsivo (celular compacto, tablet com barra lateral).
- Build: CMake, Qt for Android (NDK), CI no GitHub Actions.

## Estrutura

```
core/   include/arf/*.h  src/*.cpp  tests/
app/    main.cpp  qml/*.qml
.github/workflows/ci.yml
```

## Build

Testes do core (qualquer PC com CMake e compilador C++20):

```
cmake -S . -B build -DARF_BUILD_APP=OFF
cmake --build build && ctest --test-dir build
```

APK Android: gerado pelo CI (aba Actions, artefato `AnimatorRF-apk`).

## Roadmap

1. **Fase 1:** Home, projetos, canvas, pincéis, camadas, onion skin, timeline, frame a frame, autosave, export.
2. **Fase 2:** Quick Rig, bones, IK, poses, espelhar pose, loop, keyframes, rig + desenho por cima, bake.
3. **Fase 3:** Graph Editor, câmera, áudio, texto, cenas, biblioteca de personagens.
4. **Fase 4:** mesh skinning, smear automático, lip sync, efeitos, PSD/vídeo.
