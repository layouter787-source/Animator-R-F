#include "arf/project.h"

#include <algorithm>
#include <utility>

namespace arf {

NodeId Scene::addNode(std::string name, NodeId parent) {
  Node n;
  n.id = next_++;
  n.name = std::move(name);
  n.parent = parent;
  nodes_.push_back(std::move(n));
  return nodes_.back().id;
}

Node* Scene::find(NodeId id) {
  auto it = std::find_if(nodes_.begin(), nodes_.end(),
                         [id](const Node& n) { return n.id == id; });
  return it == nodes_.end() ? nullptr : &*it;
}

}  // namespace arf
