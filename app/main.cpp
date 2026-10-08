#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTimer>

int main(int argc, char* argv[]) {
  QGuiApplication app(argc, argv);
  app.setApplicationName("Animator-R-F");
  app.setOrganizationName("Animator-R-F");
  QQuickStyle::setStyle("Material");

  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
  engine.loadFromModule("ArfApp", "Main");

  // Teste de fumaça no CI: o QML encerra o app ao terminar; este prazo evita travar o CI.
  if (qEnvironmentVariableIsSet("ARF_SMOKE_TEST"))
    QTimer::singleShot(60000, &app, [] { QCoreApplication::exit(3); });

  return app.exec();
}
