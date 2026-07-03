// FluxingConfigEditor/YamlRoundTrip.cpp
//
// spec 024 (2026-07-03) - see YamlRoundTrip.h for the module
// contract. Implementation note: we wrap YAML::Node behind a
// pImpl-style pointer so that every translation unit that
// includes YamlRoundTrip.h does NOT have to pull in yaml-cpp
// headers (which are large and slow to compile).

#include "YamlRoundTrip.h"

#include <yaml-cpp/yaml.h>

#include <vector>

namespace fluxing {

// ---- YamlDocument ----

YamlDocument::YamlDocument() : root_(new YAML::Node()), indent_(2) {}

YamlDocument::~YamlDocument() { delete root_; }

YamlDocument::YamlDocument(YamlDocument&& other) noexcept
    : root_(other.root_), indent_(other.indent_) {
  other.root_ = new YAML::Node();
  other.indent_ = 2;
}

YamlDocument& YamlDocument::operator=(YamlDocument&& other) noexcept {
  if (this != &other) {
    delete root_;
    root_ = other.root_;
    indent_ = other.indent_;
    other.root_ = new YAML::Node();
    other.indent_ = 2;
  }
  return *this;
}

// ---- Load / Save ----

bool Load(const std::string& yaml_text, YamlDocument* out) {
  if (!out) return false;
  try {
    out->root() = YAML::Load(yaml_text);
    return true;
  } catch (const YAML::Exception& /*e*/) {
    // L27: yaml-cpp parse errors land here. The spec is
    // best-effort: return false, leave the document empty,
    // do not surface the exception text to the caller.
    return false;
  }
}

bool Save(const YamlDocument& doc, std::string* out_yaml) {
  if (!out_yaml) return false;
  YAML::Emitter emitter;
  emitter.SetIndent(doc.Indentation());
  emitter << doc.root();
  *out_yaml = emitter.c_str();
  return true;
}

// ---- Dotted-key navigation ----

namespace {

// Split a dotted key into segments. `"key_binder.bindings.0.accept"`
// becomes `["key_binder", "bindings", "0", "accept"]`.
// An empty input or a segment like `""` (consecutive dots) is
// treated as a malformed key and the function returns false.
bool SplitKey(const std::string& key, std::vector<std::string>* out) {
  if (key.empty()) return false;
  out->clear();
  size_t start = 0;
  while (start <= key.size()) {
    size_t dot = key.find('.', start);
    if (dot == std::string::npos) dot = key.size();
    if (dot == start) return false;  // empty segment
    out->push_back(key.substr(start, dot - start));
    if (dot == key.size()) return true;
    start = dot + 1;
  }
  return true;
}

}  // namespace

bool ReadString(const YamlDocument& doc,
                const std::string& key,
                std::string* out) {
  if (!out) return false;
  std::vector<std::string> segs;
  if (!SplitKey(key, &segs)) return false;
  // Map-only path: this version does not support integer sequence
  // indices. The YamlRoundTrip module's primary use case (spec 007
  // settings UI) is reading/writing map keys; sequence support is a
  // future extension once the L27 contract is settled.
  // Walk by const-ref to avoid non-const operator[] which has
  // side effects on the underlying node_data (e.g. converting
  // a sequence to a map on missing key). const operator[] is
  // pure read.
  const YAML::Node* cur = &doc.root();
  for (const auto& seg : segs) {
    if (!cur->IsMap()) return false;
    if (!(*cur)[seg]) return false;
    cur = &(*cur)[seg];
  }
  if (!cur->IsScalar()) return false;
  *out = cur->as<std::string>();
  return true;
}

bool WriteString(YamlDocument* doc,
                 const std::string& key,
                 const std::string& value) {
  if (!doc) return false;
  std::vector<std::string> segs;
  if (!SplitKey(key, &segs)) return false;
  if (segs.empty()) return false;
  // Map-only path: write through to the leaf via a chain of
  // local YAML::Node copies. yaml-cpp copies share the same
  // backing tree, so writes through the local copy mutate the
  // document.
  YAML::Node parent = doc->root();
  for (size_t i = 0; i + 1 < segs.size(); ++i) {
    const std::string& seg = segs[i];
    if (!parent.IsMap()) return false;
    if (!parent[seg]) {
      parent[seg] = YAML::Node(YAML::NodeType::Map);
    } else if (!parent[seg].IsMap()) {
      // Non-map value blocking further traversal; refuse.
      return false;
    }
    parent = parent[seg];
  }
  if (!parent.IsMap()) return false;
  parent[segs.back()] = value;
  return true;
}

}  // namespace fluxing
