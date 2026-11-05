#include "../source/ndspatch.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using namespace ndspatch;
static std::vector<char> slurp(const std::string &p) { std::vector<char> v; FILE *f = fopen(p.c_str(), "rb"); if (!f) return v; char b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, f))) v.insert(v.end(), b, b + n); fclose(f); return v; }
static bool has(const std::vector<char> &v, const char *s) { std::string t(v.begin(), v.end()); return t.find(s) != std::string::npos; }
int main(int argc, char **argv) {
    assert(argc == 3);   // wfc rom, rom without WFC strings
    std::string rom = argv[1], bare = argv[2], orig = rom + ".original";
    char err[200], warn[200];
    auto before = slurp(rom);
    assert(has(before, "nintendowifi.net"));
    assert(status(rom.c_str()) == NINTENDO);
    assert(install(rom.c_str(), "openpak.org", "1", nullptr, err, sizeof err));
    assert(status(rom.c_str()) == OPENPAK);
    auto patched = slurp(rom);
    assert(patched.size() == before.size() && !has(patched, "nintendowifi.net") && has(patched, "http://nas.openpak.org/ac"));
    assert(slurp(orig) == before);                          // exact original kept
    assert(!outdated(rom.c_str(), "openpak.org", "1"));
    assert(outdated(rom.c_str(), "openpak.org", "2"));
    assert(install(rom.c_str(), "openpak.org", "2", nullptr, err, sizeof err));   // update from the original, never stacked
    assert(slurp(orig) == before && !outdated(rom.c_str(), "openpak.org", "2"));
    // user replaces the ROM after patching: refuse, keep both
    { FILE *f = fopen(rom.c_str(), "r+b"); fseek(f, 0x4010, SEEK_SET); fputc(0x55, f); fclose(f); }
    assert(!install(rom.c_str(), "openpak.org", "3", nullptr, err, sizeof err));
    assert(!remove(rom.c_str(), warn, sizeof warn, err, sizeof err));
    assert(slurp(orig) == before);
    { FILE *f = fopen(rom.c_str(), "wb"); auto p = patched; fwrite(p.data(), 1, p.size(), f); fclose(f); }   // put the patched one back
    assert(remove(rom.c_str(), warn, sizeof warn, err, sizeof err));
    assert(slurp(rom) == before && slurp(orig).empty() && status(rom.c_str()) == NINTENDO);
    // a ROM without WFC strings is refused and nothing is left behind
    auto b0 = slurp(bare);
    assert(!install(bare.c_str(), "openpak.org", "1", nullptr, err, sizeof err));
    assert(slurp(bare) == b0 && slurp(bare + ".original").empty() && slurp(bare + ".openpak-tmp").empty());
    assert(!install(rom.c_str(), "this-domain-is-way-too-long.example", "1", nullptr, err, sizeof err));
    puts("ok");
}
