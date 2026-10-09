#pragma once
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Lista, cria e apaga projetos. Cada projeto fica numa pasta própria:
//   <dados do app>/projects/<id>/{project.arf, meta.json, thumb.png}
class ProjectStore : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_SINGLETON
  Q_PROPERTY(QVariantList projects READ projects NOTIFY projectsChanged)

public:
  explicit ProjectStore(QObject* parent = nullptr);

  QVariantList projects() const { return projects_; }

  Q_INVOKABLE void refresh();
  Q_INVOKABLE QString create(const QString& name, int width, int height, int fps, int frames);
  Q_INVOKABLE void remove(const QString& id);
  // Projeto de exemplo com traços (usado pelo teste automático do CI).
  Q_INVOKABLE QString createSample();

  static QString projectDir(const QString& id);
  static QString projectFile(const QString& id);
  static QString thumbFile(const QString& id);
  static void writeMeta(const QString& id, const QString& name, int width, int height, int fps,
                        int frames);

signals:
  void projectsChanged();

private:
  QVariantList projects_;
};
