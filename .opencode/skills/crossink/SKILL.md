---
name: crossink
description: Use when working on CrossInk ESP32-C3 e-reader firmware. Covers HAL routing, Activity lifecycle, adding settings/i18n/activities, button/input system, memory constraints, build commands, and common ESP32 gotchas. Always load at session start for CrossInk development tasks.
---

# CrossInk Development Skill

## Quick Reference

### Build Commands
```bash
pio run -e simulator          # Simulator build (fastest feedback)
pio run -e default            # Firmware build (ESP32-C3 target)
pio run -e default -t upload  # Flash to device
```

### Formatting & Lint
```bash
# Format only files you changed
find src lib include test -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Regenerate i18n after editing english.yaml
python3 scripts/gen_i18n.py
```

### Static Analysis
```bash
pio check -e default --fail-on-defect low --fail-on-defect medium --fail-on-defect high
```

## Architecture Patterns

### Activity Lifecycle
Activities are heap-allocated and deleted on exit.

```cpp
// 1. Declare in .h
class MyActivity final : public Activity {
  void onEnter() override;   // Allocate resources here
  void onExit() override;    // Free resources here (reverse order of onEnter)
  void loop() override;      // Called repeatedly, handles input
  void render(RenderLock&&) override;  // Draws to screen
};

// 2. Launch from parent activity
startActivityForResult(
    std::make_unique<MyActivity>(renderer, mappedInput, ...args...),
    [this](const ActivityResult& result) {
      if (result.isCancelled) return;
      const auto* data = std::get_if<MyResultType>(&result.data);
      // handle result
    });

// 3. Finish with result
setResult(MyResultType{...});
finish();
```

### Button Input
Always use `MappedInputManager::Button::*` enums, never raw GPIO indices:

```cpp
// In loop():
if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) { ... }
if (mappedInput.isPressed(MappedInputManager::Button::Back) &&
    mappedInput.getHeldTime() >= LONG_PRESS_MS) { ... }

// Button hints (drawn at bottom of screen)
const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
```

### File I/O
Use `FsFile` via `HalStorage`, never Arduino `File`:

```cpp
FsFile file;
if (Storage.openFileForRead("TAG", path, file)) {
  // read from file
  file.close();
}
```

### Rendering
Route through `UITheme` / `GUI` for consistent styling:

```cpp
GUI.drawHeader(renderer, rect, title);
GUI.drawList(renderer, rect, count, selectedIndex, labelFn);
GUI.drawButtonHints(renderer, ...);
CompactHeader::drawTitle(renderer, title);
```

Dimensions should come from renderer, not hardcoded:
```cpp
const auto pageWidth = renderer.getScreenWidth();
const auto pageHeight = renderer.getScreenHeight();
const auto& metrics = UITheme::getInstance().getMetrics();
```

### Singleton Stores
Pattern for persistent data stores (see `RecentBooksStore`, `TbrBooksStore`):

```cpp
// 1. Declare class inheriting PersistableStore<ClassName>
class MyStore : public PersistableStore<MyStore> {
  friend class PersistableStore<MyStore>;
  MyStore() = default;
public:
  static const char* getFilePath() { return "/.crosspoint/mydata.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);
};
#define MY_STORE MyStore::getInstance()

// 2. Load at boot in main.cpp (CRITICAL - won't persist without this)
MY_STORE.loadFromFile();

// 3. Save after mutations
MY_STORE.saveToFile();  // Called automatically by addBook/removeByPath patterns
```

## Adding New Features

### Adding a New Setting
1. Add field to `src/CrossPointSettings.h` (e.g. `uint8_t mySetting = 0;`)
2. Add `add(SettingInfo::Toggle(...))` or `add(SettingInfo::Value(...))` to `src/SettingsList.h`
3. Add to appropriate submenu builder in `SettingsList.h` (e.g. `buildSystemFilesCacheSettingsList()`)

### Adding i18n Strings
1. Add key/value to `lib/I18n/translations/english.yaml`
2. Run `python3 scripts/gen_i18n.py`
3. Use as `tr(STR_MY_KEY)` in source (logs may use hardcoded English)

### Adding a New Activity
1. Create `.h` and `.cpp` in appropriate subdirectory under `src/activities/`
2. Inherit from `Activity`, override `onEnter()`, `onExit()`, `loop()`, `render()`
3. Add launch method to `ActivityManager.h/.cpp`
4. Wire navigation from parent activity (typically `HomeActivity`)

### Adding a File Browser Action
1. Add enum value to `FileBrowserAction` in `src/activities/home/FileBrowserActionActivity.h`
2. Add item to `BookActions::buildBookActionItems()` in `src/activities/home/BookActions.cpp`
3. Handle in switch statement in `FileBrowserActivity.cpp`, `RecentBooksActivity.cpp`, `RecentBooksGridActivity.cpp`

## ESP32-C3 Memory Rules

- **Single core, 380KB RAM, no PSRAM** — every allocation counts
- Stack: keep local vars under 256 bytes, large buffers on heap
- Use `makeUniqueNoThrow<T>()` from `lib/Memory/Memory.h`, not bare `new`
- `string_view::data()` is not null-terminated — don't pass to C APIs
- ISR handlers need `IRAM_ATTR`, ISR-read data in DRAM
- Never `xSemaphoreTake()` from ISR — use ISR-safe APIs
- Large constant tables: `static const` (lives in flash, not DRAM)
- Avoid `std::string` / `String` in hot paths — prefer `char[]` + `snprintf`

## Common Gotchas

- Files opened in one context (SD card) can't be opened by another simultaneously
- `startActivityForResult` callback runs after the launched activity finishes — don't reference locals that may be destroyed
- `delay()` blocks the entire firmware — use sparingly (toast waits only)
- The simulator env uses `lib/simulator/` stubs that may differ from real hardware
- Generated files: `src/network/html/*.generated.h` and `lib/I18n/translations/*.h` — never edit directly
- `platformio.ini` `lib_ignore` is needed when libraries conflict (e.g. QRCode bool typedef vs C23)
- Commit format: `<type>: <short summary>` (feat, fix, docs, refactor, test, chore, perf)
