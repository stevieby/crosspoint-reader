#pragma once

#include <TodoistTask.h>

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

/**
 * Read-only Todoist screen. One class serves both menu entries via Mode:
 *   - Tasks: tasks due today, ordered timed-first.
 *   - Goals: tasks carrying the configured goal label, due within 7 days.
 *
 * Connects Wi-Fi on demand (like the OPDS browser), fetches over verified HTTPS,
 * and tears Wi-Fi down on exit. Credentials come from /todoist.txt.
 */
class TodoistActivity final : public Activity {
 public:
  enum class Mode { Tasks, Goals };

  explicit TodoistActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Mode mode)
      : Activity("Todoist", renderer, mappedInput), mode(mode) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  // CONFIRM_COMPLETE is shown while the yes/no prompt sub-activity is up.
  enum class State { CHECK_WIFI, WIFI_SELECTION, LOADING, READY, ERROR, CONFIRM_COMPLETE };

  Mode mode;
  State state = State::CHECK_WIFI;
  ButtonNavigator buttonNavigator;
  int selectorIndex = 0;

  std::vector<TodoistTask> tasks;
  std::string token;
  std::string goalLabel;
  bool configOk = false;
  std::string errorMessage;
  std::string statusMessage;  // shown during LOADING (initial load, sync, completing)

  void checkAndConnectWifi();
  void launchWifiSelection();
  void onWifiSelectionComplete(bool connected);
  void fetchData();
  void refresh();                  // re-sync the list (Sync button)
  void promptComplete();           // ask before completing the selected task
  void completeSelectedTask();     // POST close, then drop the row locally

  const char* headerTitle() const;
  std::string subHeaderText() const;
  std::string rowSubtitle(int index) const;
};
