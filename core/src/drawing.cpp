#include "arf/drawing.h"

#include <iterator>
#include <utility>

namespace arf {

int Animation::addLayer(std::string name) {
  Layer l;
  l.name = std::move(name);
  layers_.push_back(std::move(l));
  return int(layers_.size()) - 1;
}

Layer* Animation::layer(int index) {
  return (index >= 0 && index < int(layers_.size())) ? &layers_[index] : nullptr;
}

int Animation::keyFrameAt(int layer, int frame) const {
  if (layer < 0 || layer >= int(layers_.size())) return 0;
  const auto& keys = layers_[layer].keys;
  auto it = keys.upper_bound(frame);
  if (it == keys.begin()) return 0;
  return std::prev(it)->first;
}

const Drawing* Animation::drawingAt(int layer, int frame) const {
  const int key = keyFrameAt(layer, frame);
  if (key == 0) return nullptr;
  return &layers_[layer].keys.at(key);
}

bool Animation::hasKey(int layer, int frame) const {
  if (layer < 0 || layer >= int(layers_.size())) return false;
  return layers_[layer].keys.count(frame) > 0;
}

void Animation::insertBlankKey(int layer, int frame) {
  if (Layer* l = this->layer(layer)) l->keys[frame];  // cria vazio se não existir
}

bool Animation::addStroke(int layer, int frame, Stroke stroke) {
  Layer* l = this->layer(layer);
  if (!l || l->locked || stroke.points.empty() || frame < 1 || frame > frameCount) return false;
  const bool created = l->keys.find(frame) == l->keys.end();
  l->keys[frame].strokes.push_back(stroke);
  undo_.push_back(Action{layer, frame, std::move(stroke), created});
  if (undo_.size() > kMaxHistory) undo_.erase(undo_.begin());
  redo_.clear();
  return true;
}

bool Animation::undo() {
  if (undo_.empty()) return false;
  Action a = std::move(undo_.back());
  undo_.pop_back();
  if (Layer* l = layer(a.layer)) {
    auto it = l->keys.find(a.frame);
    if (it != l->keys.end()) {
      if (!it->second.strokes.empty()) it->second.strokes.pop_back();
      if (a.createdKey) l->keys.erase(it);
    }
  }
  redo_.push_back(std::move(a));
  return true;
}

bool Animation::redo() {
  if (redo_.empty()) return false;
  Action a = std::move(redo_.back());
  redo_.pop_back();
  if (Layer* l = layer(a.layer)) l->keys[a.frame].strokes.push_back(a.stroke);
  undo_.push_back(std::move(a));
  return true;
}

}  // namespace arf
