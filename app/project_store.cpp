#include "project_store.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <algorithm>

#include "arf/drawing.h"
#include "project_io.h"

namespace {

QString rootDir() {
  const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/projects";
  QDir().mkpath(dir);
  return dir;
}

}  // namespace

ProjectStore::ProjectStore(QObject* parent) : QObject(parent) { refresh(); }

QString ProjectStore::projectDir(const QString& id) { return rootDir() + "/" + id; }
QString ProjectStore::projectFile(const QString& id) { return projectDir(id) + "/project.arf"; }
QString ProjectStore::thumbFile(const QString& id) { return projectDir(id) + "/thumb.png"; }

void ProjectStore::writeMeta(const QString& id, const QString& name, int width, int height,
                             int fps, int frames) {
  QJsonObject o;
  o["name"] = name;
  o["width"] = width;
  o["height"] = height;
  o["fps"] = fps;
  o["frames"] = frames;
  o["modified"] = QDateTime::currentMSecsSinceEpoch();
  QFile f(projectDir(id) + "/meta.json");
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void ProjectStore::refresh() {
  struct Item {
    qint64 modified;
    QVariantMap map;
  };
  std::vector<Item> items;

  const QDir root(rootDir());
  for (const QString& id : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    QFile f(projectDir(id) + "/meta.json");
    if (!f.open(QIODevice::ReadOnly)) continue;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (o.isEmpty()) continue;

    QVariantMap m;
    m["id"] = id;
    m["name"] = o["name"].toString();
    m["width"] = o["width"].toInt();
    m["height"] = o["height"].toInt();
    m["fps"] = o["fps"].toInt();
    m["frames"] = o["frames"].toInt();
    const qint64 modified = qint64(o["modified"].toDouble());
    m["modified"] = QDateTime::fromMSecsSinceEpoch(modified).toString("dd/MM/yyyy HH:mm");
    m["thumb"] = QFile::exists(thumbFile(id)) ? QUrl::fromLocalFile(thumbFile(id)).toString() : QString();
    items.push_back({modified, m});
  }
  std::sort(items.begin(), items.end(),
            [](const Item& a, const Item& b) { return a.modified > b.modified; });

  projects_.clear();
  for (const Item& it : items) projects_ << it.map;
  emit projectsChanged();
}

QString ProjectStore::create(const QString& name, int width, int height, int fps, int frames) {
  const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  QDir().mkpath(projectDir(id));

  arf::Animation anim;
  anim.width = std::clamp(width, 64, 4096);
  anim.height = std::clamp(height, 64, 4096);
  anim.fps = std::clamp(fps, 1, 120);
  anim.frameCount = std::clamp(frames, 1, 2000);
  anim.addLayer("Camada 1");

  const QString title = name.trimmed().isEmpty() ? QStringLiteral("Sem título") : name.trimmed();
  if (!projectio::save(projectFile(id), anim, title)) return QString();
  writeMeta(id, title, anim.width, anim.height, anim.fps, anim.frameCount);
  refresh();
  return id;
}

void ProjectStore::remove(const QString& id) {
  if (id.isEmpty() || id.contains('/') || id.contains("..")) return;
  QDir(projectDir(id)).removeRecursively();
  refresh();
}
