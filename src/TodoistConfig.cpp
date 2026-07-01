#include "TodoistConfig.h"

#include <HalStorage.h>
#include <Logging.h>

namespace {
// Strip leading/trailing whitespace and CR/LF from a line.
std::string trim(const std::string& s) {
  const auto begin = s.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) return "";
  const auto end = s.find_last_not_of(" \t\r\n");
  return s.substr(begin, end - begin + 1);
}
}  // namespace

bool TodoistConfig::exists() { return Storage.exists(PATH); }

bool TodoistConfig::load() {
  apiToken.clear();
  label.clear();

  if (!Storage.exists(PATH)) {
    LOG_DBG("TODO", "No config at %s", PATH);
    return false;
  }

  const String contents = Storage.readFile(PATH);
  if (contents.isEmpty()) {
    LOG_ERR("TODO", "Empty config file");
    return false;
  }

  // Split into the first two non-empty lines: token then goal label.
  const std::string text = contents.c_str();
  size_t start = 0;
  int lineNo = 0;
  while (start <= text.size() && lineNo < 2) {
    const size_t nl = text.find('\n', start);
    const std::string raw = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
    const std::string line = trim(raw);
    if (!line.empty()) {
      if (lineNo == 0) {
        apiToken = line;
      } else {
        label = line;
      }
      ++lineNo;
    }
    if (nl == std::string::npos) break;
    start = nl + 1;
  }

  if (apiToken.empty()) {
    LOG_ERR("TODO", "Config missing API token");
    return false;
  }
  LOG_DBG("TODO", "Config loaded (label='%s')", label.c_str());
  return true;
}
