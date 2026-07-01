# Pomodoro Timer — Implementation Plan

## Problem Statement
Add a Pomodoro timer as a standalone home-menu entry. The timer runs 25-minute work sessions followed by 5-minute breaks, tracks how many sessions were completed today (persisted on SD card), and uses full-screen refresh once per minute to stay compatible with e-ink.

## Decisions Made
| Question | Decision |
|---|---|
| Location | Home menu only (like Todoist) |
| Display cadence | Full refresh once per minute |
| Durations | 25 min work / 5 min break (standard, not configurable) |
| Daily count tracking | By calendar date via `time(nullptr)` + NTP (requires clock sync) |
| Phase completion | Full-screen visual notification, user ACKs with Confirm |
| Count display location | On the PomodoroActivity screen only |
| Persistence | `.crosspoint/pomodoro.bin` on SD card |

---

## State Machine

```
IDLE ──[Confirm]──► WORK ──[25 min]──► WORK_COMPLETE ──[Confirm]──► BREAK ──[5 min]──► BREAK_COMPLETE
  ▲                   │                       │                         │                      │
  │                [Back]                  [Back]                    [Back]                 [Back]
  │                   ▼                       ▼                         ▼                      ▼
  └──────────────── IDLE ◄───────────────── IDLE ◄───────────────── IDLE ◄──────────────── IDLE
```

- **IDLE**: Displays "Pomodoro Timer", today's count, and "Start" hint. No timer running.
- **WORK**: 25-min countdown. Renders remaining minutes once per minute.  
  `preventAutoSleep()` = true to block auto-sleep.
- **WORK_COMPLETE**: Full-screen notification "Work session complete!". Increments+saves daily count.  
  Confirm → BREAK | Back → IDLE.
- **BREAK**: 5-min countdown.  
  `preventAutoSleep()` = true.
- **BREAK_COMPLETE**: Full-screen notification "Break complete!".  
  Confirm → WORK | Back → IDLE.

---

## Timer Implementation

```cpp
unsigned long startTimeMs = 0;  // millis() when current phase began
unsigned long lastRenderedMinute = 0;  // last minute boundary rendered

// In loop():
unsigned long elapsed = millis() - startTimeMs;
if (elapsed >= phaseDurationMs) { transitionToNextPhase(); return; }

unsigned long minuteNow = elapsed / 60000;
if (minuteNow != lastRenderedMinute) {
  lastRenderedMinute = minuteNow;
  requestUpdate();
}
```

- No FreeRTOS task needed. Loop polling with `millis()` is sufficient.
- `skipLoopDelay()` returns `true` when WORK or BREAK (ensures ~10ms loop granularity for transition detection).

---

## Daily Count Persistence

### File: `.crosspoint/pomodoro.bin`

```cpp
struct PomodoroFile {
  uint8_t  version = 1;   // format version
  uint16_t year;
  uint8_t  month;          // 1–12
  uint8_t  day;            // 1–31
  uint16_t count;          // completed pomodoros on this date
};  // total: 7 bytes
```

### Date resolution (X4 + X3)

Use `time(nullptr)` from `<time.h>` after NTP sync:

```cpp
static bool getTodayDate(uint16_t& year, uint8_t& month, uint8_t& day) {
    time_t now = time(nullptr);
    if (now < 1000000000UL) return false;  // not synced yet
    // Apply UTC offset from SETTINGS.clockUtcOffsetQ
    int offsetMinutes = (static_cast<int>(SETTINGS.clockUtcOffsetQ) - 48) * 15;
    now += static_cast<time_t>(offsetMinutes * 60);
    struct tm t;
    gmtime_r(&now, &t);
    year  = static_cast<uint16_t>(t.tm_year + 1900);
    month = static_cast<uint8_t>(t.tm_mon + 1);
    day   = static_cast<uint8_t>(t.tm_mday);
    return true;
}
```

If `time(nullptr)` returns < 1 billion (epoch not set — no NTP sync), the feature degrades gracefully:
- Count is still tracked in-session (in-memory) but not persisted with a valid date.
- The UI shows a note: "Clock not synced — count not saved" or simply saves with date 0 and resets on next load.

**Chosen degradation**: Save with date `{year=0, month=0, day=0}`. On next load, if date read from file is `{0,0,0}`, treat as "unknown date" and carry forward the count (don't reset). If NTP later syncs, the next completed pomodoro will update the stored date to today.

Actually simpler: if no valid date, skip the date-change check and just accumulate count. Reset only when a valid date is read and it differs from today.

---

## Screen Layout (Portrait 480×800)

```
┌─────────────────────────────┐
│  Pomodoro Timer             │  ← drawHeader()
│─────────────────────────────│
│  Work Session               │  ← drawSubHeader() (phase label)
│                             │
│         24 min              │  ← large center text (remaining minutes)
│     remaining               │
│                             │
│  Today: ● ● ● ○ ○           │  ← filled/empty circles (max 8 per row)
│  3 sessions completed       │
│                             │
│─────────────────────────────│
│  [Back: Stop] [Confirm: —]  │  ← drawButtonHints()
└─────────────────────────────┘
```

For IDLE state:
```
│         Start a             │
│         session             │
│  [Back: Home] [Confirm: Start] │
```

For WORK_COMPLETE / BREAK_COMPLETE (full screen overlay):
```
│  Work session complete!     │
│  Press Start for break      │
│  [Back: Stop] [Confirm: Break] │
```

---

## Files to Create

| File | Purpose |
|---|---|
| `src/activities/home/PomodoroActivity.h` | Activity class declaration |
| `src/activities/home/PomodoroActivity.cpp` | Activity implementation (state machine, timer, persistence) |

## Files to Modify

| File | Change |
|---|---|
| `src/activities/ActivityManager.h` | Add `HomeMenuItem::POMODORO`; add `goToPomodoro()` |
| `src/activities/ActivityManager.cpp` | Implement `goToPomodoro()`; handle `POMODORO` in `goHome()` |
| `src/activities/home/HomeActivity.h` | Add `hasPomodoroSupport` flag (always true), `onPomodoroOpen()`, update index helpers |
| `src/activities/home/HomeActivity.cpp` | Add "Pomodoro Timer" menu item, route to `onPomodoroOpen()` |
| `lib/I18n/translations/english.yaml` | Add all `STR_POMODORO_*` keys |

---

## I18n Keys to Add

```yaml
STR_POMODORO_TIMER: "Pomodoro Timer"
STR_POMODORO_WORK: "Work Session"
STR_POMODORO_BREAK: "Short Break"
STR_POMODORO_WORK_DONE: "Work session complete!"
STR_POMODORO_BREAK_DONE: "Break complete!"
STR_POMODORO_START_BREAK: "Take a break?"
STR_POMODORO_NEXT_SESSION: "Start next session?"
STR_POMODORO_TODAY_COUNT: "%d sessions today"
STR_POMODORO_MIN_REMAINING: "%d min remaining"
STR_POMODORO_START: "Start"
STR_POMODORO_STOP: "Stop"
STR_POMODORO_NEXT: "Next"
STR_POMODORO_NO_CLOCK: "Clock not synced"
```

---

## Constraints and Notes

1. **RAM**: `PomodoroActivity` has only scalar members — no heap allocations needed at all. `PomodoroFile` is 7 bytes on the stack.
2. **SPIFFS write throttle rule applies**: Write to SD only when count changes (on pomodoro completion). Not on every loop tick.
3. **`preventAutoSleep()`** = true during WORK and BREAK, false during IDLE/notification states.
4. **`skipLoopDelay()`** = true during WORK and BREAK (need fast loop for transition detection).
5. **No FreeRTOS task**: Everything on the main loop. Simple, no mutex needed.
6. **`DESTRUCTOR_CLOSES_FILE`**: Use local `HalFile` in `save()`/`load()` methods — no explicit close.
7. **`makeUniqueNoThrow`**: Not needed here — stack allocation is sufficient for 7-byte file struct.
8. **X4 clock drift note**: Document in code that date tracking is best-effort; without NTP the count resets on each day boundary only if NTP has synced.
9. **No `std::string` in hot paths**: Use `snprintf` with `char[]` buffers for all formatted text.

---

## Detailed File Changes

### 1. `lib/I18n/translations/english.yaml` — append before the last key

```yaml
STR_POMODORO_TIMER: "Pomodoro Timer"
STR_POMODORO_WORK: "Work Session"
STR_POMODORO_BREAK: "Short Break"
STR_POMODORO_WORK_DONE: "Work session complete!"
STR_POMODORO_BREAK_DONE: "Break complete!"
STR_POMODORO_START_BREAK: "Take a break?"
STR_POMODORO_NEXT_SESSION: "Start next session?"
STR_POMODORO_TODAY_COUNT: "%d sessions today"
STR_POMODORO_MIN_REMAINING: "%d min remaining"
STR_POMODORO_START: "Start"
STR_POMODORO_STOP: "Stop"
STR_POMODORO_NEXT: "Next"
STR_POMODORO_NO_CLOCK: "Clock not synced"
```

Then run: `python scripts/gen_i18n.py lib/I18n/translations lib/I18n/`

---

### 2. `src/activities/ActivityManager.h` — two changes

**In `enum class HomeMenuItem` — add `POMODORO` before `SETTINGS_MENU`**:
```cpp
// Before:
  TODOIST_GOALS,
  SETTINGS_MENU

// After:
  TODOIST_GOALS,
  POMODORO,
  SETTINGS_MENU
```

**In the public interface — add declaration after `goToTodoistGoals()`**:
```cpp
void goToPomodoro();
```

---

### 3. `src/activities/ActivityManager.cpp` — two changes

**Add include at top**:
```cpp
#include "home/PomodoroActivity.h"
```

**Add `goToPomodoro()` implementation after `goToTodoistGoals()`**:
```cpp
void ActivityManager::goToPomodoro() {
  replaceActivity(std::make_unique<PomodoroActivity>(renderer, mappedInput));
}
```

**In `goHome()` — add `POMODORO` case in the `if-else` chain**:
```cpp
} else if (activityName == "Pomodoro") {
  initialMenuItem = HomeMenuItem::POMODORO;
}
```

---

### 4. `src/activities/home/PomodoroActivity.h` — create new file

```cpp
#pragma once
#include <I18n.h>
#include <cstdint>
#include "activities/Activity.h"

class PomodoroActivity final : public Activity {
 public:
  explicit PomodoroActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Pomodoro", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  bool preventAutoSleep() override { return state == State::WORK || state == State::BREAK; }
  bool skipLoopDelay() override { return state == State::WORK || state == State::BREAK; }

 private:
  enum class State { IDLE, WORK, WORK_COMPLETE, BREAK, BREAK_COMPLETE };

  static constexpr unsigned long WORK_MS  = 25UL * 60UL * 1000UL;
  static constexpr unsigned long BREAK_MS =  5UL * 60UL * 1000UL;

  State state = State::IDLE;
  unsigned long startTimeMs = 0;
  unsigned long lastRenderedMinute = UINT32_MAX;

  // Today's count — loaded from SD, reset when date changes
  uint16_t todayCount = 0;
  uint16_t storedYear  = 0;
  uint8_t  storedMonth = 0;
  uint8_t  storedDay   = 0;

  static constexpr uint8_t POMODORO_FILE_VERSION = 1;
  static constexpr const char* POMODORO_FILE_PATH = "/.crosspoint/pomodoro.bin";

  // Returns false if time is not synced (epoch < 1 billion)
  static bool getTodayDate(uint16_t& year, uint8_t& month, uint8_t& day);

  void loadCount();
  void saveCount();

  void startPhase(State newState);
  void transitionToNextPhase();

  // Returns remaining whole minutes in the current phase
  unsigned long remainingMinutes() const;
};
```

---

### 5. `src/activities/home/PomodoroActivity.cpp` — create new file

Key implementation sections:

**getTodayDate()** — uses `time()` + `SETTINGS.clockUtcOffsetQ`:
```cpp
bool PomodoroActivity::getTodayDate(uint16_t& year, uint8_t& month, uint8_t& day) {
  time_t now = time(nullptr);
  if (now < 1000000000L) return false;
  // clockUtcOffsetQ: 48 = UTC+0; each unit = 15 min
  int offsetMinutes = (static_cast<int>(SETTINGS.clockUtcOffsetQ) - 48) * 15;
  now += static_cast<time_t>(offsetMinutes * 60);
  struct tm t{};
  gmtime_r(&now, &t);
  year  = static_cast<uint16_t>(t.tm_year + 1900);
  month = static_cast<uint8_t>(t.tm_mon + 1);
  day   = static_cast<uint8_t>(t.tm_mday);
  return true;
}
```

**loadCount()** — binary file read via HalStorage:
```cpp
void PomodoroActivity::loadCount() {
  todayCount = 0;
  storedYear = storedMonth = storedDay = 0;

  HalFile file;
  if (!Storage.openFileForRead("POMO", POMODORO_FILE_PATH, file)) return;

  struct { uint8_t version; uint16_t year; uint8_t month, day; uint16_t count; } data{};
  if (file.read(reinterpret_cast<uint8_t*>(&data), sizeof(data)) != sizeof(data)) return;
  if (data.version != POMODORO_FILE_VERSION) return;

  storedYear = data.year; storedMonth = data.month; storedDay = data.day;

  uint16_t cy; uint8_t cm, cd;
  if (getTodayDate(cy, cm, cd)) {
    if (cy == storedYear && cm == storedMonth && cd == storedDay) {
      todayCount = data.count;  // same day — carry forward
    }
    // else: different day — start at 0
  } else {
    // no NTP — carry forward regardless (can't compare dates)
    todayCount = data.count;
  }
}
```

**saveCount()** — write via HalStorage (only called on count change):
```cpp
void PomodoroActivity::saveCount() {
  uint16_t cy; uint8_t cm, cd;
  getTodayDate(cy, cm, cd);  // may return false; stored values stay {0,0,0}
  storedYear = cy; storedMonth = cm; storedDay = cd;

  struct { uint8_t version; uint16_t year; uint8_t month, day; uint16_t count; } data{
    POMODORO_FILE_VERSION, cy, cm, cd, todayCount
  };

  HalFile file;
  if (!Storage.openFileForWrite("POMO", POMODORO_FILE_PATH, file)) {
    LOG_ERR("POMO", "Could not write %s", POMODORO_FILE_PATH);
    return;
  }
  file.write(reinterpret_cast<const uint8_t*>(&data), sizeof(data));
}
```

**transitionToNextPhase()** — increments count on WORK completion:
```cpp
void PomodoroActivity::transitionToNextPhase() {
  if (state == State::WORK) {
    todayCount++;
    saveCount();
    state = State::WORK_COMPLETE;
  } else if (state == State::BREAK) {
    state = State::BREAK_COMPLETE;
  }
  requestUpdate();
}
```

**loop()** — per-minute render trigger + transition detection:
```cpp
void PomodoroActivity::loop() {
  if (state == State::WORK || state == State::BREAK) {
    const unsigned long elapsed = millis() - startTimeMs;
    const unsigned long duration = (state == State::WORK) ? WORK_MS : BREAK_MS;
    if (elapsed >= duration) {
      transitionToNextPhase();
      return;
    }
    const unsigned long minuteNow = elapsed / 60000UL;
    if (minuteNow != lastRenderedMinute) {
      lastRenderedMinute = minuteNow;
      requestUpdate();
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    switch (state) {
      case State::IDLE:          startPhase(State::WORK);  break;
      case State::WORK_COMPLETE: startPhase(State::BREAK); break;
      case State::BREAK_COMPLETE: startPhase(State::WORK); break;
      default: break;
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (state == State::IDLE) { finish(); return; }
    state = State::IDLE;
    requestUpdate();
  }
}
```

**render()** — layout per state:
```cpp
void PomodoroActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& m = UITheme::getInstance().getMetrics();
  const Rect screen = UITheme::getInstance().getScreenSafeArea(renderer, true, false);

  GUI.drawHeader(renderer,
    Rect{screen.x, screen.y + m.topPadding, screen.width, m.headerHeight},
    tr(STR_POMODORO_TIMER));

  // Sub-header: phase label
  const char* phaseLabel =
    (state == State::WORK || state == State::WORK_COMPLETE) ? tr(STR_POMODORO_WORK) :
    (state == State::BREAK || state == State::BREAK_COMPLETE) ? tr(STR_POMODORO_BREAK) : "";
  GUI.drawSubHeader(renderer,
    Rect{screen.x, screen.y + m.topPadding + m.headerHeight, screen.width, m.tabBarHeight},
    phaseLabel);

  const int contentTop = screen.y + m.topPadding + m.headerHeight + m.tabBarHeight + m.verticalSpacing;

  if (state == State::WORK || state == State::BREAK) {
    char buf[32];
    snprintf(buf, sizeof(buf), tr(STR_POMODORO_MIN_REMAINING),
             static_cast<int>(remainingMinutes()));
    // Draw large centered countdown text
    renderer.drawTextCentered(FONT_HEADING, screen.x, contentTop, screen.width, buf);
  } else if (state == State::WORK_COMPLETE) {
    renderer.drawTextCentered(FONT_HEADING, screen.x, contentTop, screen.width,
                              tr(STR_POMODORO_WORK_DONE));
    renderer.drawTextCentered(FONT_UI_MEDIUM, screen.x, contentTop + m.headerHeight,
                              screen.width, tr(STR_POMODORO_START_BREAK));
  } else if (state == State::BREAK_COMPLETE) {
    renderer.drawTextCentered(FONT_HEADING, screen.x, contentTop, screen.width,
                              tr(STR_POMODORO_BREAK_DONE));
    renderer.drawTextCentered(FONT_UI_MEDIUM, screen.x, contentTop + m.headerHeight,
                              screen.width, tr(STR_POMODORO_NEXT_SESSION));
  } else {
    // IDLE — show "Start a session" hint
    renderer.drawTextCentered(FONT_UI_MEDIUM, screen.x, contentTop, screen.width,
                              tr(STR_POMODORO_TIMER));
  }

  // Today's count
  char countBuf[32];
  snprintf(countBuf, sizeof(countBuf), tr(STR_POMODORO_TODAY_COUNT), (int)todayCount);
  const int countY = screen.y + screen.height - m.buttonHintsHeight - m.verticalSpacing - m.tabBarHeight;
  renderer.drawTextCentered(FONT_UI_MEDIUM, screen.x, countY, screen.width, countBuf);

  // Button hints
  const char* confirmLabel =
    (state == State::IDLE)           ? tr(STR_POMODORO_START) :
    (state == State::WORK_COMPLETE)  ? tr(STR_POMODORO_NEXT)  :
    (state == State::BREAK_COMPLETE) ? tr(STR_POMODORO_NEXT)  : "";
  const char* backLabel = (state == State::IDLE) ? tr(STR_BACK) : tr(STR_POMODORO_STOP);
  const auto labels = mappedInput.mapLabels(backLabel, confirmLabel, "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
```

---

### 6. `src/activities/home/HomeActivity.h` — update index helpers

In `menuItemToIndex()`, add after the Todoist block:
```cpp
if (item == HomeMenuItem::POMODORO) return i;
++i;  // (before Settings)
```

In `indexToMenuItem()`, add after Todoist block:
```cpp
if (idx == i++) return HomeMenuItem::POMODORO;
```

Add private method declaration:
```cpp
void onPomodoroOpen();
```

---

### 7. `src/activities/home/HomeActivity.cpp` — add menu entry and routing

**In `getMenuItemCount()`** — add 1 unconditionally (Pomodoro always present):
```cpp
count++;  // Pomodoro Timer (always shown)
```

**In `render()` — in the menu item building block**, add after the Todoist entries and before Settings:
```cpp
menuItems.push_back(tr(STR_POMODORO_TIMER));
menuIcons.push_back(Book);  // reuse an existing icon; or add a dedicated timer icon later
```

**In `loop()` — in the `switch` on `indexToMenuItem()`**, add case before Settings:
```cpp
case HomeMenuItem::POMODORO:
  onPomodoroOpen();
  break;
```

**Add `onPomodoroOpen()` implementation** at the bottom of the file:
```cpp
void HomeActivity::onPomodoroOpen() { activityManager.goToPomodoro(); }
```

---

## Todos

1. Add `STR_POMODORO_*` keys to `english.yaml` and regenerate I18n files
2. Add `HomeMenuItem::POMODORO` to `ActivityManager.h`; add `goToPomodoro()` declaration
3. Implement `goToPomodoro()` and `goHome()` POMODORO case in `ActivityManager.cpp`
4. Create `PomodoroActivity.h` (class declaration, State enum, all member variables)
5. Create `PomodoroActivity.cpp` (full implementation: state machine, timer, persistence, render)
6. Update `HomeActivity.h` — index helpers + `onPomodoroOpen()` declaration
7. Update `HomeActivity.cpp` — add menu entry, route to `onPomodoroOpen()`
8. Build check: `pio run`