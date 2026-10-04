#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTimer>

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  QQuickStyle::setStyle("Material");

  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
  engine.loadFromModule("AnimatorRF", "Main");

  // Teste de fumaça no CI: carrega o QML, espera um pouco e sai com sucesso.
  if (qEnvironmentVariableIsSet("ARF_SMOKE_TEST"))
    QTimer::singleShot(800, &app, &QCoreApplication::quit);

  return app.exec();
}
