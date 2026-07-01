#include "TodoistActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <TodoistQuery.h>
#include <WiFi.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "TodoistConfig.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/TodoistClient.h"

namespace {
constexpr int SUBHEADER_HEIGHT = 28;
// Reserve (subheader + spacing) when computing how many rows fit per page.
constexpr int SUBHEADER_RESERVE = 40;
}  // namespace

void TodoistActivity::onEnter() {
  Activity::onEnter();

  state = State::CHECK_WIFI;
  tasks.clear();
  selectorIndex = 0;
  errorMessage.clear();
  statusMessage = tr(STR_LOADING);

  TodoistConfig cfg;
  configOk = cfg.load();
  if (!configOk) {
    state = State::ERROR;
    errorMessage = tr(STR_TODOIST_NO_CONFIG);
    requestUpdate();
    return;
  }
  token = cfg.token();
  goalLabel = cfg.goalLabel();

  if (mode == Mode::Goals && goalLabel.empty()) {
    state = State::ERROR;
    errorMessage = tr(STR_TODOIST_NO_LABEL);
    requestUpdate();
    return;
  }

  requestUpdate();
  checkAndConnectWifi();
}

void TodoistActivity::onExit() {
  Activity::onExit();
  tasks.clear();

  // Mirror the OPDS browser: a silent restart is the firmware's way to release
  // WiFi/TLS heap cleanly without fragmenting the single contiguous block.
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void TodoistActivity::checkAndConnectWifi() {
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
    state = State::LOADING;
    statusMessage = tr(STR_LOADING);
    requestUpdate();
    fetchData();
    return;
  }
  launchWifiSelection();
}

void TodoistActivity::launchWifiSelection() {
  state = State::WIFI_SELECTION;
  requestUpdate();
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void TodoistActivity::onWifiSelectionComplete(const bool connected) {
  if (connected) {
    state = State::LOADING;
    statusMessage = tr(STR_LOADING);
    requestUpdate(true);
    fetchData();
  } else {
    state = State::ERROR;
    errorMessage = tr(STR_WIFI_CONN_FAILED);
    requestUpdate();
  }
}

void TodoistActivity::fetchData() {
  TodoistClient client(token);
  const bool ok = (mode == Mode::Tasks) ? client.fetchTodayTasks(tasks) : client.fetchGoals(goalLabel, tasks);
  if (ok) {
    state = State::READY;
    if (selectorIndex >= static_cast<int>(tasks.size())) selectorIndex = 0;
  } else {
    state = State::ERROR;
    errorMessage = tr(STR_TODOIST_FETCH_FAILED);
  }
  requestUpdate();
}

void TodoistActivity::refresh() { checkAndConnectWifi(); }

void TodoistActivity::promptComplete() {
  if (tasks.empty() || selectorIndex < 0 || selectorIndex >= static_cast<int>(tasks.size())) return;

  state = State::CONFIRM_COMPLETE;
  requestUpdate();
  startActivityForResult(
      std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_TODOIST_COMPLETE_CONFIRM),
                                             tasks[selectorIndex].content),
      [this](const ActivityResult& result) {
        state = State::READY;
        if (!result.isCancelled) {
          completeSelectedTask();
        } else {
          requestUpdate();
        }
      });
}

void TodoistActivity::completeSelectedTask() {
  if (tasks.empty() || selectorIndex < 0 || selectorIndex >= static_cast<int>(tasks.size())) return;

  state = State::LOADING;
  statusMessage = tr(STR_TODOIST_COMPLETING);
  requestUpdate(true);

  TodoistClient client(token);
  if (client.closeTask(tasks[selectorIndex].id)) {
    tasks.erase(tasks.begin() + selectorIndex);
    if (selectorIndex >= static_cast<int>(tasks.size())) {
      selectorIndex = tasks.empty() ? 0 : static_cast<int>(tasks.size()) - 1;
    }
    state = State::READY;
  } else {
    state = State::ERROR;
    errorMessage = tr(STR_TODOIST_COMPLETE_FAILED);
  }
  requestUpdate();
}

const char* TodoistActivity::headerTitle() const {
  return mode == Mode::Tasks ? tr(STR_TODOIST_TASKS) : tr(STR_TODOIST_GOALS);
}

std::string TodoistActivity::subHeaderText() const {
  char buf[48];
  const char* fmt = mode == Mode::Tasks ? tr(STR_TODOIST_DUE_COUNT) : tr(STR_TODOIST_GOALS_COUNT);
  std::snprintf(buf, sizeof(buf), fmt, static_cast<int>(tasks.size()));
  return buf;
}

std::string TodoistActivity::rowSubtitle(int index) const {
  const auto& t = tasks[index];
  std::string s;
  const auto add = [&s](const std::string& part) {
    if (part.empty()) return;
    if (!s.empty()) s += "  \xC2\xB7  ";  // middle dot (UTF-8)
    s += part;
  };

  if (t.hasTime()) add(todoist::dueTimeHHMM(t.dueDatetime));

  std::string projectSection = t.projectName;
  if (!t.sectionName.empty()) {
    projectSection = projectSection.empty() ? t.sectionName : projectSection + " / " + t.sectionName;
  }
  add(projectSection);

  if (!t.description.empty()) {
    std::string firstLine = t.description;
    const auto nl = firstLine.find('\n');
    if (nl != std::string::npos) firstLine = firstLine.substr(0, nl);
    add(firstLine);
  }
  return s;
}

void TodoistActivity::loop() {
  // A modal sub-activity (Wi-Fi picker or the complete prompt) owns input.
  if (state == State::WIFI_SELECTION || state == State::CONFIRM_COMPLETE) return;

  if (state == State::ERROR) {
    if (configOk && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      checkAndConnectWifi();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      onGoHome();
    }
    return;
  }

  if (state == State::CHECK_WIFI || state == State::LOADING) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) onGoHome();
    return;
  }

  // READY
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
    return;
  }
  // Sync on the dedicated Left front button.
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    refresh();
    return;
  }

  if (tasks.empty()) return;

  // Confirm completes the selected task (after a yes/no prompt).
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    promptComplete();
    return;
  }

  // Scroll with the side Up/Down buttons; the front Left button is reserved for
  // Sync, so we don't use the Nav* aliases (which also include Left/Right).
  const int listSize = static_cast<int>(tasks.size());
  const int pageItems =
      UITheme::getInstance().getNumberOfItemsPerPage(renderer, true, false, true, true, SUBHEADER_RESERVE);
  using Button = MappedInputManager::Button;

  buttonNavigator.onRelease({Button::Up}, [this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, listSize);
    requestUpdate();
  });
  buttonNavigator.onRelease({Button::Down}, [this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, listSize);
    requestUpdate();
  });
  buttonNavigator.onContinuous({Button::Up}, [this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(selectorIndex, listSize, pageItems);
    requestUpdate();
  });
  buttonNavigator.onContinuous({Button::Down}, [this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(selectorIndex, listSize, pageItems);
    requestUpdate();
  });
}

void TodoistActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, headerTitle());

  if (state == State::CHECK_WIFI || state == State::LOADING) {
    const char* msg = state == State::CHECK_WIFI ? tr(STR_CHECKING_WIFI)
                                                 : (statusMessage.empty() ? tr(STR_LOADING) : statusMessage.c_str());
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, msg);
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  if (state == State::ERROR) {
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 - 20, tr(STR_ERROR_MSG));
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 10, errorMessage.c_str());
    const char* retryLabel = configOk ? tr(STR_RETRY) : "";
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), retryLabel, "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // READY / CONFIRM_COMPLETE (the prompt overlays this view)
  const int subY = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  GUI.drawSubHeader(renderer, Rect{0, subY, pageWidth, SUBHEADER_HEIGHT}, subHeaderText().c_str());

  const int contentTop = subY + SUBHEADER_HEIGHT + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  if (tasks.empty()) {
    const char* empty = mode == Mode::Tasks ? tr(STR_TODOIST_NO_TASKS) : tr(STR_TODOIST_NO_GOALS);
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, empty);
  } else {
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(tasks.size()), selectorIndex,
        [this](int index) {
          const char* marker = todoist::priorityMarker(tasks[index].priority);
          return *marker ? std::string(marker) + " " + tasks[index].content : tasks[index].content;
        },
        [this](int index) { return rowSubtitle(index); });
  }

  const char* completeLabel = tasks.empty() ? "" : tr(STR_TODOIST_COMPLETE);
  const auto labels = mappedInput.mapLabels(tr(STR_HOME), completeLabel, tr(STR_SYNC), "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
