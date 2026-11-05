// OpenPak DS Patcher: a .nds app for flashcarts, DSi SD and TWiLight Menu++. Same flow and wording as the
// Switch Patcher: pick a game, the status pill shows OPENPAK / NINTENDO, Select applies, the original
// stays next to the ROM as <name>.nds.original and is restored exactly on Nintendo.
#include "ndspatch.h"
#include <algorithm>
#include <dirent.h>
#include <fat.h>
#include <nds.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif
static const char *DOMAIN = "openpak.org";

static PrintConsole top, bottom;
#define C_RESET "\x1b[39m"
#define C_OK "\x1b[32;1m"
#define C_WARN "\x1b[33;1m"
#define C_ACC "\x1b[36;1m"
#define C_MUTED "\x1b[30;1m"

struct Entry { std::string name; bool dir; };
static std::string cwd = "/";
static std::vector<Entry> entries;
static int cur = 0, scroll = 0;
static std::string status_line, note_line;

static std::string join(const std::string &d, const std::string &n) { return d == "/" ? "/" + n : d + "/" + n; }
static bool endsNds(const std::string &n) { return n.size() > 4 && strcasecmp(n.c_str() + n.size() - 4, ".nds") == 0; }

static void scan() {
    entries.clear();
    DIR *d = opendir(cwd.c_str());
    if (d) {
        while (dirent *e = readdir(d)) {
            std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            bool isDir = e->d_type == DT_DIR;
            if (isDir || endsNds(n)) entries.push_back({n, isDir});
        }
        closedir(d);
    }
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return a.dir != b.dir ? a.dir : strcasecmp(a.name.c_str(), b.name.c_str()) < 0; });
    cur = scroll = 0;
}

static void header(const char *pill, bool on) {
    consoleSelect(&top);
    consoleClear();
    iprintf(C_ACC "OpenPak DS Patcher" C_RESET "\n" C_MUTED "Network selector" C_RESET "\n");
    iprintf("--------------------------------\n");
    if (pill) iprintf("%s%s" C_RESET "\n", on ? C_OK : C_WARN, pill);
}

static void say(const char *line) { status_line = line; consoleSelect(&top); iprintf("\x1b[20;0H\x1b[K%s", line); swiWaitForVBlank(); }

enum Mode { BROWSE, ROM, UPDATE, SWITCHBACK, REBOOT } mode = BROWSE;
static int sel = 0;                 // 0 Nintendo, 1 OpenPak
static std::string romPath;

static void drawBrowse() {
    header(nullptr, false);
    iprintf("\nChoose a game (.nds):\n\n%s\n", cwd.c_str());
    consoleSelect(&bottom);
    consoleClear();
    const int rows = 20;
    if (cur < scroll) scroll = cur;
    if (cur >= scroll + rows) scroll = cur - rows + 1;
    for (int i = scroll; i < (int)entries.size() && i < scroll + rows; i++) {
        std::string n = entries[i].name + (entries[i].dir ? "/" : "");
        if (n.size() > 29) n = n.substr(0, 28) + "~";
        iprintf("%s%s\n", i == cur ? C_ACC "> " C_RESET : "  ", n.c_str());
    }
    if (entries.empty()) iprintf("  (no .nds files here)\n");
    iprintf("\x1b[22;0H" C_MUTED "A Open   B Up   Start Exit" C_RESET);
    if (!note_line.empty()) { consoleSelect(&top); iprintf("\x1b[8;0H" C_WARN "%s" C_RESET, note_line.c_str()); }
}

static void drawRom() {
    bool on = ndspatch::status(romPath.c_str()) == ndspatch::OPENPAK;
    header(on ? "[ OPENPAK ]" : "[ NINTENDO ]", on);
    std::string base = romPath.substr(romPath.rfind('/') + 1);
    if (base.size() > 30) base = base.substr(0, 29) + "~";
    iprintf(C_MUTED "%s" C_RESET "\n\n", base.c_str());
    const char *names[2] = {"Nintendo", "OpenPak"};
    for (int i = 0; i < 2; i++) {
        bool active = (i == 1) == on;
        iprintf("%s %-12s %s%s" C_RESET "\n\n", i == sel ? C_ACC ">" C_RESET : " ", names[i], i == 1 ? C_OK : C_WARN, active ? "Active" : "");
    }
    iprintf("--------------------------------\n");
    if (!status_line.empty()) iprintf("\n%s\n", status_line.c_str());
    if (!note_line.empty()) iprintf("\n" C_WARN "%s" C_RESET "\n", note_line.c_str());
    consoleSelect(&bottom);
    consoleClear();
    iprintf("\x1b[22;0H");
    if (mode == REBOOT) iprintf("A Reboot now   B Later");
    else if (mode == UPDATE || mode == SWITCHBACK) iprintf("A Yes   B No");
    else iprintf("A Select   B Back");
    iprintf(C_MUTED "  v" APP_VERSION C_RESET);
}

static void apply(bool wantOpenPak, bool again) {
    char err[200], warn[200];
    note_line.clear();
    status_line = "Working...";
    bool ok;
    if (wantOpenPak) ok = ndspatch::install(romPath.c_str(), DOMAIN, APP_VERSION, say, err, sizeof(err));
    else ok = ndspatch::remove(romPath.c_str(), warn, sizeof(warn), err, sizeof(err));
    if (!ok) { status_line = std::string("Failed: ") + err; mode = ROM; return; }
    status_line = std::string(wantOpenPak ? "OpenPak" : "Nintendo") + (again ? " setup updated. Reboot to apply?" : " selected. Reboot to apply?");
    mode = REBOOT;
}

static void openRom(const std::string &p) {
    romPath = p;
    status_line.clear(); note_line.clear();
    mode = ROM;
    sel = ndspatch::status(p.c_str()) == ndspatch::OPENPAK ? 1 : 0;
    if (ndspatch::outdated(p.c_str(), DOMAIN, APP_VERSION)) { mode = UPDATE; status_line = "OpenPak update available. Install it?"; }
}

int main(int argc, char **argv) {
    videoSetMode(MODE_0_2D); videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG); vramSetBankC(VRAM_C_SUB_BG);
    consoleInit(&top, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleInit(&bottom, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
    if (!fatInitDefault()) {
        consoleSelect(&top);
        iprintf("Failed: the SD card or flashcart\ncould not be opened.\n");
        for (;;) swiWaitForVBlank();
    }
    if (argc > 0 && argv[0]) {   // start next to the app, like most homebrew
        std::string a = argv[0];
        size_t p = a.rfind('/');
        if (p != std::string::npos && p > 0) { std::string d = a.substr(0, p); size_t c = d.find(':'); cwd = c == std::string::npos ? d : d.substr(c + 1); if (cwd.empty()) cwd = "/"; }
    }
    scan();
    bool dirty = true;
    for (;;) {
        if (dirty) { if (mode == BROWSE) drawBrowse(); else drawRom(); dirty = false; }
        swiWaitForVBlank();
        scanKeys();
        u32 k = keysDown();
        if (!k) continue;
        dirty = true;
        if (mode == BROWSE) {
            if (k & KEY_START) break;
            if (k & KEY_DOWN && cur + 1 < (int)entries.size()) cur++;
            else if (k & KEY_UP && cur > 0) cur--;
            else if (k & KEY_B) {
                if (cwd != "/") { cwd = cwd.substr(0, cwd.rfind('/')); if (cwd.empty()) cwd = "/"; scan(); } else break;
            } else if ((k & KEY_A) && !entries.empty()) {
                const Entry &e = entries[cur];
                if (e.dir) { cwd = join(cwd, e.name); scan(); }
                else openRom(join(cwd, e.name));
            }
        } else if (mode == REBOOT) {
            if (k & KEY_A) break;
            if (k & KEY_B) { mode = ROM; status_line.clear(); }
        } else if (mode == UPDATE) {
            if (k & KEY_A) apply(true, true);
            else if (k & KEY_B) { mode = SWITCHBACK; status_line = "Switch back to the official network?"; }
        } else if (mode == SWITCHBACK) {
            if (k & KEY_A) apply(false, false);
            else if (k & KEY_B) { mode = ROM; status_line.clear(); }
        } else {
            if (k & (KEY_UP | KEY_DOWN)) sel ^= 1;
            else if (k & KEY_B) { mode = BROWSE; status_line.clear(); note_line.clear(); }
            else if (k & KEY_A) {
                bool on = ndspatch::status(romPath.c_str()) == ndspatch::OPENPAK;
                apply(sel == 1, (sel == 1) == on);
            }
        }
    }
    return 0;
}
