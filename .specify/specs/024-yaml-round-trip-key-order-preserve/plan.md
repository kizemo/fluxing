# 024 - YamlRoundTrip plan (Constitution Check + verification matrix)

> Spec layer (intent): see spec.md. This plan covers the technical
> approach, Constitution Check, and verification matrix.

## 1. Technical approach

### 1.1 The YamlDocument type

The key design decision is what `YamlDocument` is. The simplest
choice that works with yaml-cpp is `YAML::Node` directly. We wrap
it in a thin struct so we can add methods (SetIndentation, etc.)
without polluting the yaml-cpp namespace:

```cpp
// FluxingConfigEditor/YamlRoundTrip.h
namespace fluxing {

class YamlDocument {
 public:
  YamlDocument();
  ~YamlDocument();
  YamlDocument(YamlDocument&&) noexcept;
  YamlDocument& operator=(YamlDocument&&) noexcept;

  YAML::Node& root() { return root_; }
  const YAML::Node& root() const { return root_; }
  void SetIndentation(int spaces) { indent_ = spaces; }
  int Indentation() const { return indent_; }

 private:
  YAML::Node root_;
  int indent_ = 2;
  // Disable copy (yaml-cpp Node copy is expensive; the spec does
  // not need copy semantics).
  YamlDocument(const YamlDocument&) = delete;
  YamlDocument& operator=(const YamlDocument&) = delete;
};

// Module-level functions (free, not class members).
bool Load(const std::string& yaml_text, YamlDocument* out);
bool Save(const YamlDocument& doc, std::string* out_yaml);
bool ReadString(const YamlDocument& doc, const std::string& key,
                std::string* out);
bool WriteString(YamlDocument* doc, const std::string& key,
                 const std::string& value);

}  // namespace fluxing
```

### 1.2 Load / Save round-trip

```cpp
bool Load(const std::string& yaml_text, YamlDocument* out) {
  if (!out) return false;
  try {
    out->root() = YAML::Load(yaml_text);
    return true;
  } catch (const YAML::Exception& e) {
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
```

`YAML::Emitter` **preserves map insertion order** when the source
is a `YAML::Node` that was loaded from a `YAML::Load` call (yaml-cpp
0.5+ behavior; this is the basis of the test). The emitter
**does not** preserve comments (yaml-cpp limitation; L27 will
record this).

### 1.3 ReadString / WriteString with dotted-key navigation

spec 007 says settings UI edits single fields; a `key` like
`"style/font_face"` is dotted. We support up to one nesting level
deeper than what `librime::config_get_string` supports — but for
v1 we mirror librime's behavior (dotted = nested map navigation):

```cpp
bool ReadString(const YamlDocument& doc, const std::string& key,
                std::string* out) {
  if (!out) return false;
  YAML::Node cur = doc.root();
  // split key by '.'
  size_t start = 0;
  while (start < key.size()) {
    size_t dot = key.find('.', start);
    std::string seg = key.substr(start, dot - start);
    if (!cur.IsMap()) return false;
    if (!cur[seg]) return false;
    cur = cur[seg];
    start = (dot == std::string::npos) ? key.size() : dot + 1;
  }
  if (!cur.IsScalar()) return false;
  *out = cur.as<std::string>();
  return true;
}
```

WriteString is the same navigation with a `cur[seg] = value` step
at the end. If a middle segment does not exist, the function
**creates** it as a map (this is what the settings UI needs: edit
a previously-untouched key, the map appears).

### 1.4 Test assertions

The 6 assertions, mirroring the L25 mock pattern:

1. **Test 1 — parser sanity**: `Load(default.yaml)` returns true;
   `root().IsMap()` is true; `root()["__patch"]["key_binder"]` is a map.
2. **Test 2 — key order**: `Save(load(default.yaml))` and the
   `key_binder.bindings` sequence's order matches the on-disk
   order (32 lines from spec 018, indices 0..30 in same order).
3. **Test 3 — round-trip equivalence**: `Load(Save(Load(x)))`
   produces the same node as `Load(x)`. This is the spec 014 / 018
   invariant closure: a consumer reading the saved yaml sees the
   same bindings in the same order.
4. **Test 4 — write preserves other keys**: `WriteString(&doc,
   "key_binder/bindings/0/send", "99")` then `Save`. The other 30
   bindings retain their position. The modified binding 0 now
   sends "99" (was "2" for the spec 014 Shift+Shift_L binding).
5. **Test 5 — dotted read**: `ReadString(doc,
   "key_binder/bindings/0/accept")` returns "Shift+Shift_L" for
   the spec 014 binding.
6. **Test 6 — comment strip** (the L27 invariant): load a yaml
   string with `# inline comment`, save, the saved text does NOT
   contain `# inline comment`. This is the documented behavior;
   the test makes the contract explicit so any future change to
   preserve comments will fail this test (the failure will be
   intentional, and the spec will be updated).

### 1.5 vcxproj layout

- New vcxproj: `test/TestYamlRoundTripE2E/TestYamlRoundTripE2E.vcxproj`
  (modeled on `test/TestBindingResolution/TestBindingResolution.vcxproj`,
  same SolutionDir override, same Boost + librime include).
- New source: `test/TestYamlRoundTripE2E/TestYamlRoundTripE2E.cpp`
  + minimal `stdafx.h` + `stdafx.cpp` + `targetver.h` (modeled
  on TestBindingResolution's stdafx set).
- `weasel.sln`: add the new project with a real (non-empty) GUID
  (L23 anti-pattern lesson — read the vcxproj's ProjectGuid and
  add exactly that to sln, no empty {}).
- `scripts/run-tests.bat`: extend the for %%P loop and the for
  %%E loop to include the new test name.

### 1.6 YamlRoundTrip module vcxproj

The `FluxingConfigEditor/YamlRoundTrip.{h,cpp}` module is a
**header + source** pair. For v0.18.14 we **do not** create a
separate `FluxingConfigEditor` vcxproj; the test project
(TestYamlRoundTripE2E) directly includes the .h and compiles
YamlRoundTrip.cpp as part of its own ClCompile. This avoids
adding a new build target and a new .lib link dependency just
for one small module. The module is small enough (estimate
<200 lines of C++) that it is more economical to fold it into
the test project. When the future settings UI (spec 025+)
needs YamlRoundTrip from a separate module, we promote it
then.

## 2. Constitution Check (per AGENTS.md R4 + spec 015/016/019 template)

| Principle | Status | Note |
|---|---|---|
| I. Intent before implementation | PASS | spec.md sec 1-2 capture goal + GWT |
| II. Test-backed change | PASS | 6 assertions cover the module API; smoke test covers installer |
| III. Spec-artifact discipline | PASS | this spec/plan/tasks three-piece set is the artifact |
| IV. Structured clarification | PASS | "no comment preservation" decision is explicit in spec sec 0 + 3 |
| V. Incremental delivery | PASS | Smallest possible slice: one module + one test |
| R1 Intent + acceptance in response | PASS | First message captures intent + GWT |
| R2 Spec layer tech-agnostic | PASS | spec.md has no yaml-cpp / vcxproj / sln names |
| R3 Single priority (P1) | PASS | tasks.md uses P1 only |
| R4 Constitution Check filled | PASS | This table |
| R5 Task granularity | PASS | 5 tasks, each <4h, 1-3 files |
| R6 Evidence before assertion | PASS | run-tests.bat output is the evidence |
| R7 One source of truth | PASS | spec 007 referenced for parent context; spec 024 supersedes implementation; no other place changes YamlRoundTrip |
| R8 Specs versioned in git | PASS | spec/plan/tasks committed in release commit |
| R9 Lookup beats memory | PASS | Read librime/dist_Win32/include/rime_api.h for config_load_string; read yaml-cpp/emitter.h for SetIndent; read existing test vcxproj as template |

## 3. Verification matrix

| Step | Command | Expected | Source of truth |
|---|---|---|---|
| Build | `scripts/run-tests.bat` | all 6 test exes build OK | AGENTS.md sec 2.3 |
| YamlRoundTrip test | `Release\TestYamlRoundTripE2E.exe` | `6 / 6 assertions passed`, exit 0 | spec 024 sec 1.4 |
| Other 5 tests | run-tests.bat | all PASS (no regression) | spec 015-019 baselines |
| Script | `scripts/run-tests.bat` exit code | 0 | spec 015 |
| Smoke test | install.nsi silent install | all layout invariants hold | AGENTS.md sec 2.5 |
| L27 lesson | lessons-learned.md | L27 entry present, follows L25/L26 shape | L25, L26 |
| Byte health | Get-Content byte scan on all changed files | 0 overlong 0xC0/0xC1, no BOM on .cpp, CRLF | AGENTS.md sec 5 step 1 |
| Lint | `clang-format -i` (CI runs) | no diff | AGENTS.md sec 2.4 |

## 4. Files changed (delta vs spec 019 release)

| File | Change | Why |
|---|---|---|
| `FluxingConfigEditor/YamlRoundTrip.h` | NEW | The module API |
| `FluxingConfigEditor/YamlRoundTrip.cpp` | NEW | The module impl |
| `test/TestYamlRoundTripE2E/TestYamlRoundTripE2E.cpp` | NEW | The 6 assertions |
| `test/TestYamlRoundTripE2E/TestYamlRoundTripE2E.vcxproj` | NEW | Test project |
| `test/TestYamlRoundTripE2E/TestYamlRoundTripE2E.vcxproj.filters` | NEW | filters (matches pattern) |
| `test/TestYamlRoundTripE2E/stdafx.h` | NEW | stdafx |
| `test/TestYamlRoundTripE2E/stdafx.cpp` | NEW | stdafx source |
| `test/TestYamlRoundTripE2E/targetver.h` | NEW | targetver |
| `weasel.sln` | + TestYamlRoundTripE2E project | L23 GUID pattern |
| `scripts/run-tests.bat` | + TestYamlRoundTripE2E in both loops | run all 6 |
| `.specify/memory/lessons-learned.md` | + L27 entry | yaml-cpp comment limitation |
| `.specify/specs/023-integration-test-yaml-roundtrip/{spec,plan,tasks}.md` | DELETED (superseded) | spec 024 is the real one |
| `.specify/specs/024-yaml-round-trip-key-order-preserve/{spec,plan,tasks}.md` | NEW | this spec |
| `CHANGELOG.md` | + [0.18.14.0-fluxing] section | release artifact |
| `release/fluxing-0.18.14.0-installer.exe` | NEW | release artifact |
| `env.bat` | 0.18.13 -> 0.18.14 (gitignored) | version bump |
| `weasel.props` | VERSION_PATCH 13 -> 14 (gitignored) | version bump |

## 5. Risks and mitigations

- **R1 (yaml-cpp ABI / linkage)**: TestBindingResolution already
  links rime.lib (which embeds yaml-cpp symbols). yaml-cpp's
  `YAML::Load` and `YAML::Emitter` are inline templates, so the
  test project only needs the headers (no link). The new
  TestYamlRoundTripE2E.vcxproj adds `$(SolutionDir)\librime\deps\yaml-cpp\include`
  to `AdditionalIncludeDirectories`. The static lib
  `librime\deps\yaml-cpp\build_Win32\Release\yaml-cpp.lib` is
  not linked (yaml-cpp templates inline the impls we use).
  Mitigation: a "compile + run a trivial `YAML::Load("a: 1")`
  then `YAML::Emitter` smoke" sub-test (Test 1 in spec 024 sec
  1.4) catches any include / link misconfig.
- **R2 (yaml emitter order)**: yaml-cpp 0.5+ preserves map
  iteration order on a Node loaded via YAML::Load. We pin this
  with Test 2 (spec 024 sec 1.4) against the actual
  `output/data/default.yaml` (32 bindings, indices 0..30).
  If a future yaml-cpp bump changes this, Test 2 fails and we
  update the assertion.
- **R3 (comment strip surprise)**: future contributors will
  expect comment preservation. The docstring in
  `YamlRoundTrip.h` and the explicit "comments are stripped"
  section in spec 024 sec 0+3 + L27 entry in lessons-learned
  all make this contract explicit. The test (Test 6) is
  intentionally a "no `# ` in saved output" assertion; if a
  future change tries to preserve comments, Test 6 fails
  intentionally and the developer knows to update the spec
  (rather than silently shipping a behavior change).
- **R4 (vcxproj template)**: L23 anti-pattern reminder —
  read the new vcxproj's actual `<ProjectGuid>` and use that
  in weasel.sln. Never let PS `Set-Content` produce an empty
  `{}` in the sln.
- **R5 (vcxproj dependencies)**: TestYamlRoundTripE2E has no
  ProjectReference to WeaselIPC or anything else; it is a
  standalone exe that just needs Boost + librime include.
  Mitigation: the vcxproj mirrors TestBindingResolution which
  is already standalone (the L25 mock pattern avoids
  WeaselIPC dep).

## 6. Out of scope

- spec 007 mac-style settings UI (deferred to a future spec)
- yaml-cpp version bump (use vendored 0.5+)
- A new vcxproj for `FluxingConfigEditor` (the module is
  compiled into the test project; promotion to a real
  library happens when the settings UI needs it)
- A new CI matrix (use existing windows-2022 build matrix)
- A coverage measurement pass (TDD sec 7 v2.1+ milestone)