// On-console port of Program.cs: patches the WFC strings of a DS ROM in place of a copy and keeps
// the untouched ROM as <name>.nds.original. Everything works on file paths so the host test
// (tests/run.sh) and the DS app share the code.
#pragma once
#include <cstddef>
#include <cstdint>

namespace ndspatch {

typedef void (*Say)(const char *line);

// The DS's replacement domain slot is as long as "nintendowifi.net".
const size_t MAX_DOMAIN = 16;

// Patches the ROM at `path` itself (no backup). True when something was replaced, false when the
// ROM holds nothing to patch, and false with err set on a hard failure.
bool patchInPlace(const char *path, const char *domain, Say say, char *err, size_t errLen);

enum State { NINTENDO, OPENPAK };
State status(const char *path);                 // OPENPAK when this app patched the ROM and kept its original
bool outdated(const char *path, const char *domain, const char *version);   // patched by another domain/version
bool install(const char *path, const char *domain, const char *version, Say say, char *err, size_t errLen);
bool remove(const char *path, char *warn, size_t warnLen, char *err, size_t errLen);

}  // namespace ndspatch
