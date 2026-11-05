// Host helper: patch a ROM in place with the on-console code (used by the differential test).
#include "../source/ndspatch.h"
#include <cstdio>
static void say(const char *l) { fprintf(stderr, "%s\n", l); }
int main(int argc, char **argv) {
    if (argc < 4) return 2;
    char err[200] = "";
    bool hit = ndspatch::patchInPlace(argv[2], argv[3], say, err, sizeof(err));
    if (err[0]) { fprintf(stderr, "ERR %s\n", err); return 1; }
    return hit ? 0 : 3;
}
