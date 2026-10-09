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
#include <cmath>

#include "arf/drawing.h"
#include "mypaint_engine.h"
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

QString ProjectStore::createSample() {
  const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  QDir().mkpath(projectDir(id));

  arf::Animation anim;
  anim.width = 1280;
  anim.height = 720;
  anim.fps = 24;
  anim.frameCount = 48;
  anim.addLayer("Camada 1");
  anim.addLayer("Camada 2");

  // Um pincel MyPaint, se o build tiver o motor.
  std::string preset;
  if (mp::available()) {
    if (mp::hasPreset("classic/pencil")) preset = "classic/pencil";
    else if (!mp::presets().isEmpty())
      preset = mp::presets().first().toMap().value("id").toString().toStdString();
  }

  for (int f = 1; f <= anim.frameCount; ++f) {
    arf::Stroke pencil;  // lápis: diagonal que anda a cada quadro
    pencil.size = 10;
    pencil.color = 0xFF2F6FB0;
    for (int k = 0; k <= 20; ++k)
      pencil.points.push_back({100.0f + 12.0f * f + 15.0f * k, 100.0f + 20.0f * k, 1.0f, 0.01f * k});
    anim.addStroke(0, f, pencil);

    arf::Stroke ink;  // tinta: pressão variando ao longo do traço
    ink.brush = arf::BrushType::Ink;
    ink.size = 16;
    for (int k = 0; k <= 30; ++k)
      ink.points.push_back({200.0f + 25.0f * k, 420.0f + 40.0f * std::sin(0.3f * k + 0.2f * f),
                            0.2f + 0.8f * std::sin(float(M_PI) * k / 30.0f), 0.01f * k});
    anim.addStroke(1, f, ink);

    if (!preset.empty()) {
      arf::Stroke paint;
      paint.preset = preset;
      paint.size = 14;
      paint.color = 0xFFC0392B;
      for (int k = 0; k <= 30; ++k)
        paint.points.push_back({150.0f + 28.0f * k, 560.0f + 30.0f * std::cos(0.25f * k + 0.2f * f),
                                0.6f, 0.01f * k});
      anim.addStroke(1, f, paint);
    }
  }

  const QString title = QStringLiteral("Exemplo");
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
