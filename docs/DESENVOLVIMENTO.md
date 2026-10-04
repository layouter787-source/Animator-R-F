# Desenvolvimento

- `main`: versão estável. Só recebe código que passou no CI.
- `dev`: onde o trabalho acontece. A Pull Request `dev` → `main` roda o CI (testes do core, teste de fumaça do QML e build do APK).
- Versões do Qt/NDK: `ci/config.env`.
- Scripts de CI: `scripts/ci-smoke.sh` e `scripts/ci-android.sh`.
