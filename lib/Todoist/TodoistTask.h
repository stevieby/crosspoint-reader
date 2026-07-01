#pragma once

#include <string>

// A single Todoist task, trimmed to the fields the reader displays. Strings are
// owned so the parsed JSON document can be freed immediately after extraction.
struct TodoistTask {
  std::string id;
  std::string content;      // task title
  std::string description;  // optional notes
  int priority = 1;         // Todoist API: 4 = highest (p1) ... 1 = normal (p4)
  std::string projectId;
  std::string sectionId;
  std::string projectName;   // resolved from the /projects map
  std::string sectionName;   // resolved from the /sections map
  std::string dueDate;       // "YYYY-MM-DD" (date-only tasks)
  std::string dueDatetime;   // "YYYY-MM-DDTHH:MM:SS..." when the task has a time

  // A task is "timed" when Todoist gave it a specific time of day.
  bool hasTime() const { return !dueDatetime.empty(); }
};
