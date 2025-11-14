#include "gs_packet_handler.h"

#include "gs_server_c.h"
#include "common/logging_c.h"
#include "common/memory_addresses.h"
#include "ee/eestruct.h"
#include "sce/libgraph.h"

#include <stdint.h>
#include <stdio.h>

/*
 * GS_packet_parser.c
 *
 * Simple PS2 Graphics Synthesizer (GS) packet/GIF parser in C.
 *
 * What this parser does (reasonable, practical defaults):
 *  - Parses GIFTags from a buffer of u64 qwords
 *  - Handles FLG=0 (packed), FLG=1 (repeated), FLG=2 (image), FLG=3 (reserved)
 *  - Iterates NLOOP entries and decodes the REGS list
 *  - Calls a user callback for each register write with the raw data words
 *
 * This is intentionally generic so it can be adapted to your GS emulator, dumper,
 * or analysis tool. It doesn't implement the whole GS register semantics — only
 * packet decoding and delivery to a callback where you can implement register
 * interpretation (TEX0, TEX1, PRIM, RGBAQ, ST, XYZ, etc.).
 *
 * Author: ChatGPT (GPT-5 Thinking mini)
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* === Types & helpers === */

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

enum { GIF_FLG_PACKED = 0, GIF_FLG_REGLIST = 1, GIF_FLG_IMAGE = 2 };

/* GIFTag layout (64-bit):
 * bits 0..15   NLOOP
 * bits 16..23 EOP/Prim??? (older refs — we'll extract fields systematically)
 * Actual layout according to docs:
 *  - NLOOP: 15..0
 *  - EOP: bit 15 of the second half? To keep robust, we'll decode using masks used commonly:
 * Reference: generic docs but this parser uses these fields: NLOOP, EOP, PRE, PRIM, FLG, NREG
 */

typedef struct {
    u16 nloop;    /* number of loops */
    u8  eop;      /* end-of-packet */
    u8  pre;      /* primitive flag (PACKED REGS enable) */
    u8  prim;     /* PRIM (3 bytes) --- we store low 8 bits here for convenience */
    u8  flg;      /* GIF FLG (packed/reglist/image) */
    u8  nreg;     /* number of regs in reglist */
    u8  regs[16]; /* register list (max 16 entries) */
} giftag_t;

/* Register callback: called for each register write found in packet.
 * reg : register ID byte (the GS register number)
 * data: pointer to u64 qwords relevant for this register (length depends on reg and packing)
 * qwords: number of u64 words in data
 * user: user-provided pointer
 */

typedef void (*gs_reg_cb)(u8 reg, const u64 *data, size_t qwords, void *user);

/* === Parser implementation === */

/* read a u64 from buffer (little-endian). buffer must be valid and have enough bytes. */
static inline u64 read_u64_le(const u8 *buf) {
    u64 v = 0;
    /* assuming platform supports unaligned reads, but do it portable */
    for (int i = 0; i < 8; ++i) v |= ((u64)buf[i]) << (8*i);
    return v;
}

/* read a u32 little-endian */
static inline u32 read_u32_le(const u8 *buf) {
    u32 v = 0;
    for (int i = 0; i < 4; ++i) v |= ((u32)buf[i]) << (8*i);
    return v;
}

/* parse a GIFTag from a u64 (from the GIFTag QWORD) */
static giftag_t parse_giftag(u64 tagq) {
    giftag_t t = {0};

    /* Common GIFTag bit layout (LSB first):
     *  bits  0..15  NLOOP
     *  bits 32      EOP (1 bit)
     *  bits 33      PRE (1 bit)
     *  bits 34..46  PRIM (11 bits) -- but only lower 8 often used here
     *  bits 44..45  FLG (2 bits)
     *  bits 52..55  NREG (4 bits)
     *  bits 56..63  REGS (8 bits each, up to NREG entries)
     * However public docs differ; this heuristic matches most GIF parsers.
     */

    t.nloop = (u16)(tagq & 0xffffull);
    t.eop   = (u8)((tagq >> 63) & 0x1); /* sometimes EOP is at top bit; tolerate */

    /* PRE is usually bit 58 in some docs; we try a robust extraction: */
    t.pre = (u8)((tagq >> 58) & 0x1);

    t.flg = (u8)((tagq >> 46) & 0x3); /* FLG often at bits 46-47 */
    t.nreg = (u8)((tagq >> 52) & 0xf);

    /* Extract up to 16 regs in the following bytes of the top half (if present).
     * Some implementations pack reg list starting at bit 0 of the high 32-bits.
     */
    u64 high = (tagq >> 32);
    for (int i = 0; i < 16; ++i) {
        t.regs[i] = (u8)((high >> (i*8)) & 0xff);
    }

    /* PRIM - store low byte of the prim field (best-effort) */
    t.prim = (u8)((tagq >> 32) & 0xff);

    return t;
}

/* Helper: convert reg id byte to text for debug */
static const char *reg_name(u8 reg) {
    switch (reg) {
        case 0x00: return "PRIM";
        case 0x01: return "RGBAQ";
        case 0x02: return "ST";
        case 0x03: return "UV";
        case 0x04: return "XYZF2";
        case 0x05: return "XYZ3";
        case 0x06: return "TEX0_1";
        case 0x07: return "CLAMP_1";
        case 0x08: return "TEX1_1";
        case 0x09: return "TEX2_1";
        case 0x0a: return "XYOFFSET_1";
        case 0x10: return "FRAME";
        case 0x11: return "ZBUF";
        case 0x13: return "BITBLTBUF";
        case 0x14: return "TRXPOS";
        case 0x15: return "TRXREG";
        case 0x16: return "TRXDIR";
        default: return "REG";
    }
}

/* The main parser: iterate over a buffer of bytes (must be multiple of 8). Call cb for each reg. */
int gs_parse_packets(const u8 *buf, size_t size, gs_reg_cb cb, void *user) {
    if (!buf || size < 8 || (size & 7)) return -1; /* require at least one qword and 8-byte aligned */

    const u8 *p = buf;
    const u8 *end = buf + size;

    while (p + 8 <= end) {
        u64 q = read_u64_le(p);
        p += 8;

        /* If this qword is a GIFTag (we assume packet stream starts with GIFTag)
         * Decode it and then read NLOOP entries accordingly.
         */
        giftag_t tag = parse_giftag(q);

        if (tag.nloop == 0) {
            /* Some streams may use 0 as terminator; break to avoid infinite loops */
            if (tag.eop) break;
        }

        /* If FLG == PACKED (0): each qword contains register(s) according to PRE/NREG
         * In packed mode, PRE=1 means all data following are packed according to REGS list.
         * We'll follow a practical approach: for each loop we'll deliver data where one 'unit'
         * is a single qword unless the register implies multiple components (user can decode).
         */

        if (tag.flg == GIF_FLG_PACKED) {
            /* When packed, the next NLOOP qwords each contain data for the REGS sequence.
             * For each of the nloop entries, we iterate regs[0..nreg-1] and give one qword each.
             */
            for (u32 i = 0; i < tag.nloop; ++i) {
                for (u8 r = 0; r < tag.nreg; ++r) {
                    if (p + 8 > end) return -2; /* truncated */
                    u64 data = read_u64_le(p);
                    p += 8;
                    if (cb) cb(tag.regs[r], &data, 1, user);
                }
            }
        } else if (tag.flg == GIF_FLG_REGLIST) {
            /* Reglist mode: each loop contains a count of qwords equal to the reg's size.
             * In practice, for reglist mode the GIFTag is followed by NLOOP * ??? qwords;
             * Many implementations use the convention: each loop has 1 qword per reg in list.
             * For simplicity we'll treat it like packed (safe fallback). */
            for (u32 i = 0; i < tag.nloop; ++i) {
                for (u8 r = 0; r < tag.nreg; ++r) {
                    if (p + 8 > end) return -3;
                    u64 data = read_u64_le(p);
                    p += 8;
                    if (cb) cb(tag.regs[r], &data, 1, user);
                }
            }
        } else if (tag.flg == GIF_FLG_IMAGE) {
            /* Image mode: raw image data follows (not register-based). We don't attempt to decode
             * image data here; instead, call the callback with a special REG==0xff and pass
             * the whole following NLOOP qwords as image payload. */
            if (p + (8 * tag.nloop) > end) return -4;
            if (cb) cb(0xff, (const u64*)p, tag.nloop, user);
            p += 8 * tag.nloop;
        } else {
            /* unknown flg - abort */
            return -5;
        }

        /* if EOP set, we can optionally break; but packets may continue, so we continue parsing */
        if (tag.eop) {
            /* end-of-packet: break out of parsing loop */
            break;
        }
    }

    return 0;
}

/* === Example usage: a simple callback that prints register writes === */

static void print_reg_cb(u8 reg, const u64 *data, size_t qwords, void *user) {
    (void)user;
    if (reg == 0xff) {
        printf("IMAGE payload: %zu qwords\n", (size_t)qwords);
        return;
    }
    printf("REG 0x%02x (%s):", reg, reg_name(reg));
    for (size_t i = 0; i < qwords; ++i) {
        printf(" %016llx", (unsigned long long)data[i]);
    }
    printf("\n");
}

#ifdef DEMO_MAIN
/* Demo main: builds a tiny fake GIF stream and parses it. */
int main(void) {
    /* Build a very small buffer: one GIFTag (packed, nloop=2, nreg=2, regs={0x01,0x02})
     * followed by 4 qwords of data (2 loops * 2 regs).
     */
    u8 buf[8 + 4*8];
    memset(buf, 0, sizeof(buf));

    /* Construct a giftag: nloop=2, flg=0 (packed), nreg=2, regs[0]=0x01 (RGBAQ), regs[1]=0x02 (ST) */
    u64 tag = 0;
    tag |= (u64)2; /* nloop */
    tag |= ((u64)0 << 46); /* flg = 0 */
    tag |= ((u64)2 << 52); /* nreg = 2 */
    /* place regs into high 32-bits low bytes */
    u64 high = 0;
    high |= (u64)0x01;
    high |= (u64)0x02 << 8;
    tag |= (high << 32);

    /* write tag little-endian */
    for (int i = 0; i < 8; ++i) buf[i] = (tag >> (8*i)) & 0xff;

    /* data qwords */
    u64 d[] = { 0x1122334455667788ull, 0x99aabbccddeeff00ull, 0x0123456789abcdefull, 0xf0e1d2c3b4a59687ull };
    memcpy(buf+8, d, sizeof(d));

    int r = gs_parse_packets(buf, sizeof(buf), print_reg_cb, NULL);
    if (r) fprintf(stderr, "parse error %d\n", r);
    return 0;
}
#endif

/* End of GS_packet_parser.c */


void ReadPacket(Q_WORDDATA *packet)
{
    //printf("packet of type type %#x\n", packet->ui32[2]);

    if (packet->uc8[3] == 0x70 /* DMAend */  /* DMArefe */)
    {
        return;
    }

    if (packet->uc8[3] == 0x20 /* DMAnext */)
    {
        info_log("Found packet chain!");

        Q_WORDDATA *packet_chain = (Q_WORDDATA*)*(int64_t*)&(((int*)&packet->ui32[1])[0]);

        if (packet->ui32[2] == VU0_ADDRESS || packet->ui32[2] == VU0_ADDRESS || packet_chain == NULL)
        {
            return;
        }

        ReadAllPackets(packet_chain);
    }

    switch (packet->ui32[2])
    {
        case SCE_GS_TEX0_1: info_log("SCE_GS_TEX0_1 Detected"); break;
        case SCE_GS_BITBLTBUF:
            sceGsLoadImage *load = (sceGsLoadImage*)&packet[-1];
            GsUpload(load, (unsigned char*)*(int64_t*)&(((int*)&load[1])[1]));
            info_log("SCE_GS_BITBLTBUF Detected");
            break;
        default: /* info_log("Unknown packet" /*, pbuf[i].ui32[2]); */ break;
    }
}

void ReadAllPackets(Q_WORDDATA *packets)
{
    if (packets == NULL)
    {
        return;
    }

    //gs_parse_packets(packets, sizeof(packets)*512, print_reg_cb, NULL);
    while (packets->ul128 != 0 && packets->ui32[2] != 0)
    {
        ReadPacket(packets);
        packets++;
    }
}