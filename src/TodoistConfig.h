#pragma once

#include <string>

/**
 * Reads the Todoist credentials from a plain-text file in the SD-card root.
 *
 * Format of /todoist.txt:
 *   line 1: API token (personal device — stored in clear text)
 *   line 2: goal label name (bare, no leading '@')
 *
 * Surrounding whitespace and CR/LF are trimmed. The label line is optional and
 * only used by the Goals screen.
 */
class TodoistConfig {
 public:
  static constexpr const char* PATH = "/todoist.txt";

  // True when the config file is present on the SD card.
  static bool exists();

  // Load token and goal label from PATH. Returns true only if a non-empty token
  // was read.
  bool load();

  const std::string& token() const { return apiToken; }
  const std::string& goalLabel() const { return label; }

 private:
  std::string apiToken;
  std::string label;
};
