#include "TodoistQuery.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace todoist {

std::string buildTodayQuery() { return "today"; }

std::string buildGoalsQuery(const std::string& label) {
  // Todoist filter: tasks carrying the label and due within the next 7 days.
  // "7 days" includes today through the next six days.
  return "@" + label + " & 7 days";
}

std::string urlEncode(const std::string& s) {
  std::string out;
  out.reserve(s.size() * 3);
  for (const unsigned char c : s) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += static_cast<char>(c);
    } else {
      char buf[4];
      std::snprintf(buf, sizeof(buf), "%%%02X", c);
      out += buf;
    }
  }
  return out;
}

const char* priorityMarker(int priority) {
  switch (priority) {
    case 4:
      return "!!!";
    case 3:
      return "!!";
    case 2:
      return "!";
    default:
      return "";
  }
}

std::string dueTimeHHMM(const std::string& datetime) {
  const auto tPos = datetime.find('T');
  if (tPos == std::string::npos || tPos + 6 > datetime.size()) return "";
  // Expect "...THH:MM..."; validate the HH:MM shape before returning it.
  const std::string hhmm = datetime.substr(tPos + 1, 5);
  if (hhmm.size() != 5 || hhmm[2] != ':' || !std::isdigit(static_cast<unsigned char>(hhmm[0])) ||
      !std::isdigit(static_cast<unsigned char>(hhmm[1])) || !std::isdigit(static_cast<unsigned char>(hhmm[3])) ||
      !std::isdigit(static_cast<unsigned char>(hhmm[4]))) {
    return "";
  }
  return hhmm;
}

void sortTasks(std::vector<TodoistTask>& tasks) {
  std::stable_sort(tasks.begin(), tasks.end(), [](const TodoistTask& a, const TodoistTask& b) {
    if (a.hasTime() != b.hasTime()) return a.hasTime();          // timed before untimed
    if (a.hasTime()) return a.dueDatetime < b.dueDatetime;        // earliest time first
    return a.priority > b.priority;                              // untimed: highest priority first
  });
}

}  // namespace todoist
