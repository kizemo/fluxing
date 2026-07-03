// FluxingConfigEditor/YamlRoundTrip.h
//
// spec 024 (2026-07-03) - A small wrapper around yaml-cpp that
// round-trips a YAML document while preserving the **key order**
// of the source file. Comments are NOT preserved (L27 — yaml-cpp
// 0.5+ does not store comments in YAML::Node; preserving them
// requires a custom yaml tokenizer, which is out of scope for
// v0.18.14).
//
// This module is a standalone subset of spec 007 (yaml-config-ui).
// The full spec 007 mac-style settings UI is deferred to a future
// spec; this module is independently useful for any code that
// needs to read and write rime/weasel YAML files without losing
// the on-disk key ordering of sections like `key_binder.bindings`.

#pragma once

#include <string>

// Forward-declare yaml-cpp's Node so we don't pull the yaml-cpp
// headers into every translation unit that includes this header.
namespace YAML {
class Node;
}

namespace fluxing {

class YamlDocument {
 public:
  YamlDocument();
  ~YamlDocument();
  YamlDocument(YamlDocument&& other) noexcept;
  YamlDocument& operator=(YamlDocument&& other) noexcept;

  // Access the underlying yaml-cpp node. Mutating it is allowed
  // (the spec 007 settings UI will mutate via WriteString, which
  // is implemented in terms of these accessors).
  YAML::Node& root() { return *root_; }
  const YAML::Node& root() const { return *root_; }

  // Default indentation is 2 spaces, matching the rime/weasel
  // style guide and the existing output\data\*.yaml files.
  void SetIndentation(int spaces) { indent_ = spaces; }
  int Indentation() const { return indent_; }

 private:
  // yaml-cpp Node copy is expensive and not needed. Move-only.
  YamlDocument(const YamlDocument&) = delete;
  YamlDocument& operator=(const YamlDocument&) = delete;

  YAML::Node* root_;  // pImpl-style: avoid exposing yaml-cpp Node layout
  int indent_;
};

// Parse a YAML string into a YamlDocument. Returns true on success.
// On failure (malformed YAML, yaml-cpp exception), returns false
// and the YamlDocument is left in an empty state. Comments are
// stripped on load (L27).
bool Load(const std::string& yaml_text, YamlDocument* out);

// Serialize a YamlDocument back to a YAML string. Returns true on
// success. Key order is preserved. Comments are NOT emitted
// (L27 — yaml-cpp does not retain them on the parsed Node tree).
bool Save(const YamlDocument& doc, std::string* out_yaml);

// Read a string value at the given dotted key. `key` may contain
// `.` separators (e.g. `"key_binder.bindings.0.accept"`).
// Returns false if the key does not exist or the resolved node
// is not a scalar.
bool ReadString(const YamlDocument& doc,
                const std::string& key,
                std::string* out);

// Write a string value at the given dotted key. If intermediate
// maps do not exist, they are created. Existing non-map values
// along the path cause the function to return false (we will
// not silently overwrite a scalar with a map). Returns true on
// success.
bool WriteString(YamlDocument* doc,
                 const std::string& key,
                 const std::string& value);

}  // namespace fluxing