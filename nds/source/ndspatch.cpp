#include "ndspatch.h"
#include "blz.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace ndspatch {

typedef std::vector<uint8_t> Buf;

// ---------------------------------------------------------------- file helpers
struct File {
    FILE *f;
    explicit File(const char *p, const char *mode) : f(fopen(p, mode)) {}
    ~File() { if (f) fclose(f); }
    bool ok() const { return f != nullptr; }
    bool seek(uint32_t o) { return fseek(f, (long)o, SEEK_SET) == 0; }
    bool read(void *b, size_t n) { return n == 0 || fread(b, 1, n, f) == n; }
    bool write(const void *b, size_t n) { return n == 0 || fwrite(b, 1, n, f) == n; }
    uint32_t u32(uint32_t at) { uint8_t b[4] = {0, 0, 0, 0}; seek(at); read(b, 4); return b[0] | (b[1] << 8) | (b[2] << 16) | ((uint32_t)b[3] << 24); }
    bool putU32(uint32_t at, uint32_t v) { uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)}; return seek(at) && write(b, 4); }
    bool putU16(uint32_t at, uint16_t v) { uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)}; return seek(at) && write(b, 2); }
    bool fill(uint32_t at, uint8_t v, uint32_t n) { if (!seek(at)) return false; for (uint32_t i = 0; i < n; i++) if (fputc(v, f) == EOF) return false; return true; }
};

static uint32_t fileSize(const char *p) {
    FILE *f = fopen(p, "rb");
    if (!f) return 0xFFFFFFFFu;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n < 0 ? 0xFFFFFFFFu : (uint32_t)n;
}
static bool exists(const char *p) { return fileSize(p) != 0xFFFFFFFFu; }

static uint16_t crc16(File &f, uint32_t at, uint32_t len) {
    uint16_t crc = 0xFFFF;
    f.seek(at);
    for (uint32_t i = 0; i < len; i++) {
        int c = fgetc(f.f);
        if (c == EOF) c = 0;
        crc ^= (uint8_t)c;
        for (int j = 0; j < 8; j++) crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
    }
    return crc;
}

// ---------------------------------------------------------------- replacement
static std::vector<size_t> locate(const uint8_t *d, size_t n, const std::string &s) {
    std::vector<size_t> r;
    if (s.empty() || s.size() > n) return r;
    for (size_t i = 0; i + s.size() <= n; i++)
        if (d[i] == (uint8_t)s[0] && memcmp(d + i, s.data(), s.size()) == 0) r.push_back(i);
    return r;
}

static std::string asciiAt(const uint8_t *d, size_t n, size_t at) {
    std::string s;
    for (size_t i = at; i < n && d[i] != 0; i++) s += d[i] > 0x7F ? '?' : (char)d[i];   // like Encoding.ASCII
    return s;
}

static std::string replaceAll(std::string s, const std::string &a, const std::string &b) {
    size_t p = 0;
    while ((p = s.find(a, p)) != std::string::npos) { s.replace(p, a.size(), b); p += b.size(); }
    return s;
}

static bool replaceOne(uint8_t *data, size_t n, const std::string &search, const std::string &replace, uint8_t pad, bool extra) {
    bool replaced = false;
    int requiredPadding = (int)search.size() - (int)replace.size();
    for (size_t at : locate(data, n, search)) {
        std::string orig = asciiAt(data, n, at);
        if (orig == "https://") continue;   // lone https is a strcmp probe, not a URL
        std::string rep = replaceAll(orig, search, replace);
        size_t total = rep.size() + (requiredPadding > 0 ? (size_t)requiredPadding : 0);
        if (at + total > n) continue;   // would run off the section
        replaced = true;
        size_t i = 0;
        for (; i < rep.size(); i++) data[at + i] = (uint8_t)rep[i];
        for (; i < rep.size() + (size_t)(requiredPadding > 0 ? requiredPadding : 0); i++) data[at + i] = pad;
        if (extra && at + i < n) data[at + i] = 0x7F;   // keeps a recompressed binary the original size
    }
    return replaced;
}

static bool replaceInData(uint8_t *data, size_t n, const char *domain, uint8_t pad, bool extra) {
    bool r = replaceOne(data, n, "https://", "http://", pad, extra);
    if (domain && *domain) r = replaceOne(data, n, "nintendowifi.net", domain, pad, extra) || r;
    return r;
}

static void removeDebugStrings(uint8_t *data, size_t n) {
    static const char *strs[] = {"recv buffer size", "send buffer size", "unknown connect mode", "Split packet parse error",
                                 "NULL byte expected!", "Processing adderror packet", "Out of memory.", " buf->buffer"};
    for (const char *s : strs) {
        std::string t(s);
        for (size_t at : locate(data, n, t)) for (size_t i = 0; i < t.size(); i++) data[at + i] = 0x20;
    }
}

static bool removeStringsInKnownGames(const char *gamecode, uint8_t *data, size_t n) {
    if (strncmp(gamecode, "VI2J", 4) != 0) return false;   // FE12 English patch
    for (uint32_t i = 0x1047C8; i < 0x1047C8 + 0x4A4 && i < n; i++) if (data[i] != 0) data[i] = 0x20;
    return true;
}

// ---------------------------------------------------------------- sections
static void say(Say s, const char *line) { if (s) s(line); }

static bool patchArm9(File &nds, uint32_t pos, uint32_t len, const char *domain, Say sy, char *err, size_t el) {
    Buf data(len);
    if (!nds.seek(pos) || !nds.read(data.data(), len)) { snprintf(err, el, "Could not read the ROM"); return false; }
    uint32_t compressedSize = len >= 8 ? (data[len - 8] | (data[len - 7] << 8) | (data[len - 6] << 16)) : 0;
    uint8_t headerLength = len >= 8 ? data[len - 5] : 0;
    uint32_t additional = len >= 8 ? (data[len - 4] | (data[len - 3] << 8) | (data[len - 2] << 16) | ((uint32_t)data[len - 1] << 24)) : 0;
    uint32_t decompressedSize = additional + len;

    bool compressed = false;
    uint8_t *dec = nullptr;
    uint32_t decLen = len;
    bool hdrOk = headerLength >= 8 && headerLength <= 11;
    bool sizeOk = len >= compressedSize + 0x4000 && len <= compressedSize + 0x400B;
    if (hdrOk && sizeOk) {
        uint32_t dl = 0;
        uint8_t *d = blz::decode(data.data(), len, &dl);
        if (d && dl == decompressedSize) { compressed = true; dec = d; decLen = dl; }
        else free(d);
    }
    if (!compressed) { dec = data.data(); decLen = len; }

    // A buffer from blz::decode is freed on every exit.
    struct Free { uint8_t *p; bool own; ~Free() { if (own) free(p); } } guard = {dec, compressed};

    if (!replaceInData(dec, decLen, domain, 0x00, true)) return false;

    uint32_t gc = nds.u32(0x0C);
    char gamecode[5] = {(char)gc, (char)(gc >> 8), (char)(gc >> 16), (char)(gc >> 24), 0};
    Buf out;
    uint32_t newSize = len;
    if (compressed) {
        say(sy, "Replacing and recompressing ARM9...");
        uint32_t nl = 0;
        uint8_t *e = blz::encode(dec, decLen, true, false, &nl);
        if (!e) { snprintf(err, el, "Not enough memory to recompress ARM9"); return false; }
        if (nl > len) {
            // bigger: redo without the extra byte after each string, from the original
            free(e);
            uint32_t dl2 = 0;
            uint8_t *d2 = blz::decode(data.data(), len, &dl2);
            if (!d2) { snprintf(err, el, "Not enough memory to recompress ARM9"); return false; }
            free(guard.p); guard.p = d2; dec = d2; decLen = dl2;
            replaceInData(dec, decLen, domain, 0x00, false);
            e = blz::encode(dec, decLen, true, true, &nl);
            if (!e) { snprintf(err, el, "Not enough memory to recompress ARM9"); return false; }
            if (nl > len) {
                free(e);
                if (!removeStringsInKnownGames(gamecode, dec, decLen)) removeDebugStrings(dec, decLen);
                e = blz::encode(dec, decLen, true, true, &nl);
                if (!e) { snprintf(err, el, "Not enough memory to recompress ARM9"); return false; }
                if (nl > len) say(sy, "Warning: ARM9 grew; the game may not start");
            }
        }
        out.assign(e, e + nl);
        free(e);
        newSize = nl;
        if (newSize != len) {
            // the compressed size also sits in the secure area: find and update it
            bool found = false;
            for (uint32_t i = 0; i + 4 <= 0x4000 && i + 4 <= out.size(); i += 4) {
                uint32_t m = out[i] | (out[i + 1] << 8) | (out[i + 2] << 16) | ((uint32_t)out[i + 3] << 24);
                if (m == len + 0x02000000u || m == len + 0x02004000u) {
                    uint32_t v = (m == len + 0x02004000u) ? newSize + 0x02004000u : newSize + 0x02000000u;
                    out[i] = (uint8_t)v; out[i + 1] = (uint8_t)(v >> 8); out[i + 2] = (uint8_t)(v >> 16); out[i + 3] = (uint8_t)(v >> 24);
                    found = true;
                    break;
                }
            }
            if (!found) say(sy, "Warning: ARM9 size not found in the secure area");
        }
    } else {
        say(sy, "Replacing ARM9...");
        out.assign(dec, dec + decLen);
        newSize = decLen;
    }

    if (!nds.seek(pos) || !nds.write(out.data(), out.size())) { snprintf(err, el, "Could not write the ROM"); return false; }
    int diff = (int)len - (int)newSize;
    uint32_t cursor = pos + newSize;
    if (diff > 0) {
        if (nds.u32(pos + len) == 0xDEC00621) {
            uint8_t footer[12];
            nds.seek(pos + len);
            nds.read(footer, 12);
            nds.seek(pos + newSize);
            nds.write(footer, 12);
            cursor = pos + newSize + 12;
        }
        if (!nds.fill(cursor, 0xFF, (uint32_t)diff)) { snprintf(err, el, "Could not write the ROM"); return false; }
    }
    if (!nds.putU32(0x2C, newSize)) { snprintf(err, el, "Could not write the ROM"); return false; }
    uint16_t sc = crc16(nds, pos, 0x4000);
    nds.putU16(0x6C, sc);
    uint16_t hc = crc16(nds, 0, 0x15E);
    nds.putU16(0x15E, hc);
    return true;
}

static bool patchArm7(File &nds, uint32_t pos, uint32_t len, const char *domain, Say sy, char *err, size_t el) {
    Buf data(len);
    if (!nds.seek(pos) || !nds.read(data.data(), len)) { snprintf(err, el, "Could not read the ROM"); return false; }
    if (!replaceInData(data.data(), len, domain, 0x00, false)) return false;
    say(sy, "Replacing ARM7...");
    if (!nds.seek(pos) || !nds.write(data.data(), len)) { snprintf(err, el, "Could not write the ROM"); return false; }
    return true;
}

static bool patchOverlay(File &nds, uint32_t pos, uint32_t len, const char *domain, Say sy, char *err, size_t el, bool *failed) {
    uint32_t fatOffset = nds.u32(0x48);
    bool modified = false;
    for (uint32_t i = 0; i + 0x20 <= len; i += 0x20) {
        uint32_t id = nds.u32(pos + i);
        uint32_t compressedWord = nds.u32(pos + i + 0x1C);
        uint32_t compressedSize = compressedWord & 0x00FFFFFF;
        uint8_t mask = (uint8_t)(compressedWord >> 24);
        uint32_t start = nds.u32(fatOffset + 8 * id), end = nds.u32(fatOffset + 8 * id + 4);
        uint32_t size = end - start;
        if (size == 0 || size > 0x1000000 || end < start) continue;
        Buf data(size);
        if (!nds.seek(start) || !nds.read(data.data(), size)) { snprintf(err, el, "Could not read the ROM"); *failed = true; return modified; }

        bool compressed = (mask & 1) != 0;
        uint8_t *dec = data.data();
        uint32_t decLen = size;
        uint8_t *owned = nullptr;
        if (compressed) {
            uint32_t dl = 0;
            owned = blz::decode(data.data(), size, &dl);
            if (owned) { dec = owned; decLen = dl; }
            else { say(sy, "Warning: an overlay could not be decompressed"); compressed = false; }
        }
        bool hit = replaceInData(dec, decLen, domain, 0x00, false);
        if (hit) {
            modified = true;
            Buf out;
            int diff;
            if (compressed) {
                say(sy, "Replacing and recompressing an overlay...");
                uint32_t nl = 0;
                uint8_t *e = blz::encode(dec, decLen, false, false, &nl);
                if (e && (int)size - (int)nl < 0) {
                    free(e);
                    removeDebugStrings(dec, decLen);
                    e = blz::encode(dec, decLen, false, true, &nl);
                }
                if (!e) { free(owned); snprintf(err, el, "Not enough memory to recompress an overlay"); *failed = true; return modified; }
                out.assign(e, e + nl);
                free(e);
                if (compressedSize == size) {
                    uint32_t w = (compressedWord & 0xFF000000u) | (nl & 0x00FFFFFF);
                    nds.putU32(pos + i + 0x1C, w);
                }
            } else {
                say(sy, "Replacing an overlay...");
                out.assign(dec, dec + decLen);
            }
            diff = (int)size - (int)out.size();
            if (!nds.seek(start) || !nds.write(out.data(), out.size())) { free(owned); snprintf(err, el, "Could not write the ROM"); *failed = true; return modified; }
            uint32_t newEnd = start + (uint32_t)out.size();
            if (diff > 0 && !nds.fill(newEnd, 0xFF, (uint32_t)diff)) { free(owned); snprintf(err, el, "Could not write the ROM"); *failed = true; return modified; }
            nds.putU32(fatOffset + 8 * id + 4, newEnd);
        }
        free(owned);
    }
    return modified;
}

bool patchInPlace(const char *path, const char *domain, Say sy, char *err, size_t el) {
    err[0] = '\0';
    File nds(path, "r+b");
    if (!nds.ok()) { snprintf(err, el, "Could not open the ROM"); return false; }
    uint32_t size = fileSize(path);
    if (size < 0x200) { snprintf(err, el, "This is not a DS ROM"); return false; }
    uint32_t a9o = nds.u32(0x20), a9s = nds.u32(0x2C), a7o = nds.u32(0x30), a7s = nds.u32(0x3C);
    if (a9o + a9s > size || a7o + a7s > size) { snprintf(err, el, "This is not a DS ROM"); return false; }
    say(sy, "Patching ARM executables...");
    bool failed = false;
    bool m9 = patchArm9(nds, a9o, a9s, domain, sy, err, el);
    if (err[0]) return false;
    bool m7 = patchArm7(nds, a7o, a7s, domain, sy, err, el);
    if (err[0]) return false;
    say(sy, "Patching overlays...");
    uint32_t o9o = nds.u32(0x50), o9l = nds.u32(0x54), o7o = nds.u32(0x58), o7l = nds.u32(0x5C);
    bool v9 = o9l && o9o + o9l <= size ? patchOverlay(nds, o9o, o9l, domain, sy, err, el, &failed) : false;
    if (failed) return false;
    bool v7 = o7l && o7o + o7l <= size ? patchOverlay(nds, o7o, o7l, domain, sy, err, el, &failed) : false;
    if (failed) return false;
    return m9 || m7 || v9 || v7;
}

// ---------------------------------------------------------------- backup / state
static std::string side(const char *p) { return std::string(p) + ".openpak"; }
static std::string orig(const char *p) { return std::string(p) + ".original"; }
static std::string tmpName(const char *p) { return std::string(p) + ".openpak-tmp"; }

static uint32_t crcStep(uint32_t crc, const uint8_t *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        crc ^= b[i];
        for (int j = 0; j < 8; j++) crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
    return crc;
}

// Size, then CRC32 over the header, ARM9 and ARM7: what the patch changes in the main binaries.
static bool fingerprint(const char *path, uint32_t *size, uint32_t *crc) {
    *size = fileSize(path);
    File f(path, "rb");
    if (!f.ok() || *size < 0x200) return false;
    uint32_t a9o = f.u32(0x20), a9s = f.u32(0x2C), a7o = f.u32(0x30), a7s = f.u32(0x3C);
    uint32_t c = 0xFFFFFFFFu;
    struct R { uint32_t o, n; } rs[3] = {{0, 0x200}, {a9o, a9s}, {a7o, a7s}};
    uint8_t buf[2048];
    for (auto &r : rs) {
        if (r.o + r.n > *size) return false;
        f.seek(r.o);
        for (uint32_t done = 0; done < r.n;) {
            uint32_t k = r.n - done < sizeof(buf) ? r.n - done : sizeof(buf);
            if (!f.read(buf, k)) return false;
            c = crcStep(c, buf, k);
            done += k;
        }
    }
    *crc = ~c;
    return true;
}

struct Side { uint32_t size = 0, crc = 0; std::string domain, version; bool ok = false; };
static Side readSide(const char *path) {
    Side s;
    FILE *f = fopen(side(path).c_str(), "rb");
    if (!f) return s;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        if (!strncmp(line, "size=", 5)) s.size = (uint32_t)strtoul(line + 5, nullptr, 10);
        else if (!strncmp(line, "crc=", 4)) s.crc = (uint32_t)strtoul(line + 4, nullptr, 16);
        else if (!strncmp(line, "domain=", 7)) s.domain = line + 7;
        else if (!strncmp(line, "version=", 8)) s.version = line + 8;
    }
    fclose(f);
    s.ok = true;
    return s;
}
static bool writeSide(const char *path, uint32_t size, uint32_t crc, const char *domain, const char *version) {
    std::string t = side(path) + ".tmp";
    FILE *f = fopen(t.c_str(), "wb");
    if (!f) return false;
    fprintf(f, "size=%lu\ncrc=%08lx\ndomain=%s\nversion=%s\n", (unsigned long)size, (unsigned long)crc, domain, version);
    bool ok = fclose(f) == 0;
    if (!ok) { ::remove(t.c_str()); return false; }
    ::remove(side(path).c_str());
    return ::rename(t.c_str(), side(path).c_str()) == 0;
}

static bool copyFile(const char *from, const char *to, Say sy) {
    File in(from, "rb");
    if (!in.ok()) return false;
    FILE *out = fopen(to, "wb");
    if (!out) return false;
    static uint8_t buf[32768];
    uint32_t total = fileSize(from), done = 0;
    size_t n;
    bool ok = true;
    int lastPct = -1;
    while ((n = fread(buf, 1, sizeof(buf), in.f)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { ok = false; break; }
        done += (uint32_t)n;
        int pct = total ? (int)((uint64_t)done * 100 / total) : 100;
        if (sy && pct / 10 != lastPct / 10) { char l[40]; snprintf(l, sizeof(l), "Copying %d%%", pct); sy(l); lastPct = pct; }
    }
    ok = (fclose(out) == 0) && ok;
    if (!ok) { ::remove(to); return false; }
    return fileSize(to) == total;
}

State status(const char *path) {
    Side s = readSide(path);
    return s.ok && exists(orig(path).c_str()) ? OPENPAK : NINTENDO;
}

bool outdated(const char *path, const char *domain, const char *version) {
    Side s = readSide(path);
    return s.ok && exists(orig(path).c_str()) && (s.domain != domain || s.version != version);
}

bool install(const char *path, const char *domain, const char *version, Say sy, char *err, size_t el) {
    err[0] = '\0';
    if (strlen(domain) > MAX_DOMAIN) { snprintf(err, el, "The domain is longer than 16 characters"); return false; }
    std::string o = orig(path), t = tmpName(path);
    Side s = readSide(path);
    bool haveOrig = exists(o.c_str());
    if (s.ok && haveOrig) {
        uint32_t sz, crc;
        if (!fingerprint(path, &sz, &crc) || sz != s.size || crc != s.crc) {
            snprintf(err, el, "ROM changed since patching; original kept as .original");
            return false;
        }
    }
    const char *src = haveOrig ? o.c_str() : path;
    say(sy, "Reading and copying the ROM...");
    if (!copyFile(src, t.c_str(), sy)) { ::remove(t.c_str()); snprintf(err, el, "Could not copy the ROM (is the card full?)"); return false; }
    char perr[160] = "";
    bool hit = patchInPlace(t.c_str(), domain, sy, perr, sizeof(perr));
    if (!hit) {
        ::remove(t.c_str());
        snprintf(err, el, "%s", perr[0] ? perr : "This game does not use Nintendo WFC");
        return false;
    }
    uint32_t sz, crc;
    if (!fingerprint(t.c_str(), &sz, &crc)) { ::remove(t.c_str()); snprintf(err, el, "Could not verify the patched ROM"); return false; }
    // swap: keep the first original, then put the patched copy in place
    if (haveOrig) {
        if (::remove(path) != 0 && exists(path)) { ::remove(t.c_str()); snprintf(err, el, "Could not replace the ROM"); return false; }
    } else {
        if (::rename(path, o.c_str()) != 0) { ::remove(t.c_str()); snprintf(err, el, "Could not back up the ROM"); return false; }
    }
    if (::rename(t.c_str(), path) != 0) {
        // put the original back rather than leave no ROM
        if (!haveOrig) ::rename(o.c_str(), path);
        else copyFile(o.c_str(), path, nullptr);
        ::remove(t.c_str());
        snprintf(err, el, "Could not write the patched ROM; original restored");
        return false;
    }
    writeSide(path, sz, crc, domain, version);
    return true;
}

bool remove(const char *path, char *warn, size_t wl, char *err, size_t el) {
    err[0] = warn[0] = '\0';
    std::string o = orig(path);
    if (!exists(o.c_str())) { snprintf(err, el, "No original found next to this ROM"); return false; }
    Side s = readSide(path);
    uint32_t sz, crc;
    bool exact = s.ok && fingerprint(path, &sz, &crc) && sz == s.size && crc == s.crc;
    if (!exact && exists(path)) {
        snprintf(err, el, "ROM changed since patching; original kept as .original");
        (void)wl;
        return false;
    }
    if (exists(path) && ::remove(path) != 0) { snprintf(err, el, "Could not remove the patched ROM"); return false; }
    if (::rename(o.c_str(), path) != 0) { snprintf(err, el, "Could not restore the original; it is kept as .original"); return false; }
    ::remove(side(path).c_str());
    return true;
}

}  // namespace ndspatch
