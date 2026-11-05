#include "blz.h"
#include <cstdlib>
#include <cstring>

namespace blz {

static const uint32_t THRESHOLD = 2, N = 0x1002, F = 0x12, RAW_MAXIM = 0x00FFFFFF;

static void invert(uint8_t *b, uint32_t start, uint32_t length) {
    if (length == 0) return;
    uint32_t bottom = start + length - 1;
    while (start < bottom) { uint8_t c = b[start]; b[start++] = b[bottom]; b[bottom--] = c; }
}

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

uint8_t *decode(uint8_t *pak_buffer, uint32_t pak_len, uint32_t *outLen) {
    if (pak_len < 8) return nullptr;
    uint32_t inc_len = rd32(pak_buffer + pak_len - 4);
    if (inc_len == 0) return nullptr;
    uint32_t hdr_len = pak_buffer[pak_len - 5];
    if (hdr_len < 8 || hdr_len > 11 || pak_len <= hdr_len) return nullptr;
    uint32_t enc_len = rd32(pak_buffer + pak_len - 8) & 0x00FFFFFF;
    if (enc_len > pak_len) return nullptr;
    uint32_t dec_len = pak_len - enc_len;
    uint32_t plen = enc_len - hdr_len;
    uint32_t raw_len = dec_len + enc_len + inc_len;
    if (raw_len > RAW_MAXIM) return nullptr;
    uint8_t *raw_buffer = (uint8_t *)calloc(raw_len ? raw_len : 1, 1);
    if (!raw_buffer) return nullptr;
    uint32_t pak = 0, raw = 0, pak_end = dec_len + plen, raw_end = raw_len;
    for (uint32_t len = 0; len < dec_len; len++) raw_buffer[raw++] = pak_buffer[pak++];
    invert(pak_buffer, dec_len, plen);
    uint8_t flags = 0, mask = 0;
    while (raw < raw_end) {
        mask >>= 1;
        if (mask == 0) {
            if (pak == pak_end) break;
            flags = pak_buffer[pak++];
            mask = 0x80;
        }
        if ((flags & mask) == 0) {
            if (pak == pak_end) break;
            raw_buffer[raw++] = pak_buffer[pak++];
        } else {
            if (pak + 1 >= pak_end) break;
            uint32_t pos = pak_buffer[pak++] << 8;
            pos |= pak_buffer[pak++];
            uint32_t len = (pos >> 12) + THRESHOLD + 1;
            if (raw + len > raw_end) len = raw_end - raw;
            pos = (pos & 0xFFF) + 3;
            if (pos > raw) { free(raw_buffer); invert(pak_buffer, dec_len, plen); return nullptr; }
            while (len-- != 0) { raw_buffer[raw] = raw_buffer[raw - pos]; raw++; }
        }
    }
    invert(pak_buffer, dec_len, plen);   // put the input back
    invert(raw_buffer, dec_len, raw_len - dec_len);
    *outLen = raw;
    return raw_buffer;
}

static uint16_t crc16(const uint8_t *b, uint32_t off, uint32_t len) {
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= b[off++];
        for (int i = 0; i < 8; i++) crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
    }
    return crc;
}

// Hash-chain match finder over the (inverted) buffer. Returns the same (len, pos) as the
// original SEARCH loop: pos ascending from 3, a candidate wins only with a strictly longer match.
struct Finder {
    static const uint32_t HBITS = 15, RING = 8192;
    uint32_t *head, *prev;
    const uint8_t *buf;
    uint32_t total, inserted;
    bool ok;
    Finder(const uint8_t *b, uint32_t n) : buf(b), total(n), inserted(0) {
        head = (uint32_t *)malloc(sizeof(uint32_t) << HBITS);
        prev = (uint32_t *)malloc(sizeof(uint32_t) * RING);
        ok = head && prev;
        if (ok) memset(head, 0xFF, sizeof(uint32_t) << HBITS);
    }
    ~Finder() { free(head); free(prev); }
    static uint32_t hash(const uint8_t *p) { return ((p[0] << 8) ^ (p[1] << 4) ^ p[2] ^ (p[0] << 2)) & ((1u << HBITS) - 1); }
    void insertUpTo(uint32_t raw) {   // positions below raw become searchable
        while (inserted < raw) {
            if (inserted + 2 < total) {
                uint32_t h = hash(buf + inserted);
                prev[inserted & (RING - 1)] = head[h];
                head[h] = inserted;
            }
            inserted++;
        }
    }
    void search(uint32_t raw, uint32_t raw_end, uint32_t *l, uint32_t *p) {
        *l = THRESHOLD;
        if (raw + 2 >= total) return;
        insertUpTo(raw);
        uint32_t maxp = raw >= N ? N : raw;
        uint32_t cand = head[hash(buf + raw)];
        while (cand != 0xFFFFFFFFu) {
            uint32_t pos = raw - cand;
            if (pos > maxp) break;
            if (pos >= 3) {
                uint32_t len;
                for (len = 0; len < F; len++) {
                    if (raw + len == raw_end) break;
                    if (len >= pos) break;
                    if (buf[raw + len] != buf[raw + len - pos]) break;
                }
                if (len > *l) { *p = pos; if ((*l = len) == F) break; }
            }
            if (inserted - cand > RING - 2) break;   // chain entry already recycled
            uint32_t nx = prev[cand & (RING - 1)];
            if (nx != 0xFFFFFFFFu && nx >= cand) break;
            cand = nx;
        }
    }
};

uint8_t *encode(uint8_t *raw_buffer, uint32_t raw_len, bool arm9, bool quiet, uint32_t *outLen) {
    (void)quiet;
    uint32_t pak_len = raw_len + ((raw_len + 7) / 8) + 11;
    uint8_t *pak_buffer = (uint8_t *)calloc(pak_len, 1);
    if (!pak_buffer) return nullptr;
    uint32_t pak_tmp = 0, raw_tmp = raw_len, raw_new = raw_len;
    if (arm9 && raw_len >= 0x4000) {
        uint16_t crc = crc16(raw_buffer, 0x10, 0x07F0);
        raw_buffer[0x0E] = (uint8_t)(crc & 0xFF);
        raw_buffer[0x0F] = (uint8_t)(crc >> 8);
        raw_new -= 0x4000;
    }
    invert(raw_buffer, 0, raw_len);
    Finder fnd(raw_buffer, raw_len);
    if (!fnd.ok) { free(pak_buffer); invert(raw_buffer, 0, raw_len); return nullptr; }

    uint32_t pak = 0, raw = 0, raw_end = raw_new, flg = 0, len_best = 0, pos_best = 0;
    uint8_t mask = 0;
    while (raw < raw_end) {
        mask >>= 1;
        if (mask == 0) { flg = pak++; pak_buffer[flg] = 0; mask = 0x80; }
        fnd.search(raw, raw_end, &len_best, &pos_best);
        pak_buffer[flg] <<= 1;
        if (len_best > THRESHOLD) {
            raw += len_best;
            pak_buffer[flg] |= 1;
            pak_buffer[pak++] = (uint8_t)(((len_best - (THRESHOLD + 1)) << 4) | ((pos_best - 3) >> 8));
            pak_buffer[pak++] = (uint8_t)((pos_best - 3) & 0xFF);
        } else {
            pak_buffer[pak++] = raw_buffer[raw++];
        }
        if (pak + raw_len - raw < pak_tmp + raw_tmp) { pak_tmp = pak; raw_tmp = raw_len - raw; }
    }
    while (mask != 0 && mask != 1) { mask >>= 1; pak_buffer[flg] <<= 1; }
    pak_len = pak;
    invert(raw_buffer, 0, raw_len);
    invert(pak_buffer, 0, pak_len);

    uint8_t *result;
    if (pak_tmp == 0 || raw_len + 4 < ((pak_tmp + raw_tmp + 3) & ~3u) + 8) {
        // not worth compressing: raw copy plus a zero footer
        pak = 0;
        for (raw = 0; raw < raw_len; raw++) pak_buffer[pak++] = raw_buffer[raw];
        while (pak & 3) pak_buffer[pak++] = 0;
        pak_buffer[pak] = pak_buffer[pak + 1] = pak_buffer[pak + 2] = pak_buffer[pak + 3] = 0;
        pak += 4;
        result = pak_buffer;
    } else {
        uint8_t *tmp = (uint8_t *)calloc(raw_tmp + pak_tmp + 11, 1);
        if (!tmp) { free(pak_buffer); return nullptr; }
        memcpy(tmp, raw_buffer, raw_tmp);
        memcpy(tmp + raw_tmp, pak_buffer + pak_len - pak_tmp, pak_tmp);
        free(pak_buffer);
        pak = raw_tmp + pak_tmp;
        uint32_t enc_len = pak_tmp, hdr_len = 8, inc_len = raw_len - pak_tmp - raw_tmp;
        while (pak & 3) { tmp[pak++] = 0xFF; hdr_len++; }
        uint32_t v = enc_len + hdr_len;
        tmp[pak] = v & 0xFF; tmp[pak + 1] = (v >> 8) & 0xFF; tmp[pak + 2] = (v >> 16) & 0xFF;
        pak += 3;
        tmp[pak++] = (uint8_t)hdr_len;
        v = inc_len - hdr_len;
        tmp[pak] = v & 0xFF; tmp[pak + 1] = (v >> 8) & 0xFF; tmp[pak + 2] = (v >> 16) & 0xFF; tmp[pak + 3] = (v >> 24) & 0xFF;
        pak += 4;
        result = tmp;
    }
    *outLen = pak;
    return result;
}

}  // namespace blz
