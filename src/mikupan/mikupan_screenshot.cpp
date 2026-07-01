#include "mikupan_screenshot.h"
#include "mikupan_config.h"
#include "debug/mikupan_logging_c.h"
#include "mikupan/rendering/mikupan_renderer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int g_screenshot_pending = 0;

static unsigned int s_crc_table[256];
static int          s_crc_table_built = 0;

static void crc32_build_table(void)
{
    for (unsigned int i = 0; i < 256; i++)
    {
        unsigned int c = i;
        for (int k = 0; k < 8; k++)
        {
            c = (c & 1u) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
        }
        s_crc_table[i] = c;
    }
    s_crc_table_built = 1;
}

static unsigned int crc32_update(unsigned int crc, const unsigned char *buf, size_t len)
{
    if (!s_crc_table_built) crc32_build_table();
    for (size_t i = 0; i < len; i++)
    {
        crc = s_crc_table[(crc ^ buf[i]) & 0xffu] ^ (crc >> 8);
    }
    return crc;
}

static unsigned int adler32_compute(const unsigned char *buf, size_t len)
{
    unsigned int a = 1, b = 0;
    const unsigned int MOD = 65521u;
    for (size_t i = 0; i < len; i++)
    {
        a = (a + buf[i]) % MOD;
        b = (b + a) % MOD;
    }
    return (b << 16) | a;
}

static void put_u32_be(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)((v >> 24) & 0xff);
    p[1] = (unsigned char)((v >> 16) & 0xff);
    p[2] = (unsigned char)((v >>  8) & 0xff);
    p[3] = (unsigned char)((v >>  0) & 0xff);
}

static int write_chunk(FILE *f, const char *type4, const unsigned char *data, size_t len)
{
    unsigned char hdr[8];
    put_u32_be(hdr, (unsigned int) len);
    memcpy(hdr + 4, type4, 4);
    if (fwrite(hdr, 1, 8, f) != 8) return 0;
    if (len > 0 && fwrite(data, 1, len, f) != len) return 0;

    unsigned int crc = 0xffffffffu;
    crc = crc32_update(crc, (const unsigned char *) type4, 4);
    if (len > 0) crc = crc32_update(crc, data, len);
    crc ^= 0xffffffffu;

    unsigned char crc_be[4];
    put_u32_be(crc_be, crc);
    return fwrite(crc_be, 1, 4, f) == 4;
}

int MikuPan_ScreenshotWritePng(const char *path, const unsigned char *rgba,
                               int width, int height)
{
    if (path == NULL || rgba == NULL || width <= 0 || height <= 0)
    {
        return 0;
    }

    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;

    // PNG signature
    static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (fwrite(sig, 1, 8, f) != 8) { fclose(f); return 0; }

    // IHDR
    unsigned char ihdr[13];
    put_u32_be(ihdr + 0, (unsigned int) width);
    put_u32_be(ihdr + 4, (unsigned int) height);
    ihdr[8]  = 8;  // bit depth
    ihdr[9]  = 2;  // color type: truecolor
    ihdr[10] = 0;  // compression method (must be 0)
    ihdr[11] = 0;  // filter method (must be 0)
    ihdr[12] = 0;  // interlace: none
    if (!write_chunk(f, "IHDR", ihdr, sizeof(ihdr))) { fclose(f); return 0; }

    // Build the filtered scan-line buffer (filter byte 0 = "None" per row).
    const size_t src_row_bytes  = (size_t) width * 4u;
    const size_t row_bytes      = (size_t) width * 3u;
    const size_t filtered_size  = (size_t) height * (1u + row_bytes);
    unsigned char *filtered = (unsigned char *) malloc(filtered_size);
    if (filtered == NULL) { fclose(f); return 0; }
    for (int y = 0; y < height; y++)
    {
        unsigned char *dst = filtered + (size_t) y * (1u + row_bytes);
        const unsigned char *src = rgba + (size_t) y * src_row_bytes;
        dst[0] = 0;
        for (int x = 0; x < width; x++)
        {
            dst[1 + (size_t) x * 3u + 0u] = src[(size_t) x * 4u + 0u];
            dst[1 + (size_t) x * 3u + 1u] = src[(size_t) x * 4u + 1u];
            dst[1 + (size_t) x * 3u + 2u] = src[(size_t) x * 4u + 2u];
        }
    }

    // Wrap filtered data in a zlib stream of stored (uncompressed) DEFLATE
    // blocks. Each stored block carries up to 65535 bytes.
    const size_t MAX_BLOCK = 65535u;
    size_t num_blocks = (filtered_size + MAX_BLOCK - 1) / MAX_BLOCK;
    if (num_blocks == 0) num_blocks = 1;

    size_t zlib_cap = 2 /* CMF+FLG */ + filtered_size + num_blocks * 5u + 4 /* adler */;
    unsigned char *zlib = (unsigned char *) malloc(zlib_cap);
    if (zlib == NULL) { free(filtered); fclose(f); return 0; }

    size_t zo = 0;
    // CMF=0x78 (deflate, 32K window). FLG chosen so (CMF*256+FLG) % 31 == 0.
    zlib[zo++] = 0x78;
    zlib[zo++] = 0x01;

    size_t in_pos   = 0;
    size_t remaining = filtered_size;
    do
    {
        size_t chunk = remaining > MAX_BLOCK ? MAX_BLOCK : remaining;
        int last = (chunk == remaining);
        zlib[zo++] = (unsigned char)(last ? 0x01 : 0x00); // BFINAL + BTYPE=00
        zlib[zo++] = (unsigned char)(chunk & 0xff);
        zlib[zo++] = (unsigned char)((chunk >> 8) & 0xff);
        unsigned short nlen = (unsigned short) (~((unsigned short) chunk));
        zlib[zo++] = (unsigned char)(nlen & 0xff);
        zlib[zo++] = (unsigned char)((nlen >> 8) & 0xff);
        if (chunk > 0)
        {
            memcpy(zlib + zo, filtered + in_pos, chunk);
            zo += chunk;
            in_pos += chunk;
            remaining -= chunk;
        }
    } while (remaining > 0);

    unsigned int adler = adler32_compute(filtered, filtered_size);
    put_u32_be(zlib + zo, adler);
    zo += 4;

    int ok = write_chunk(f, "IDAT", zlib, zo);

    free(zlib);
    free(filtered);

    if (ok) ok = write_chunk(f, "IEND", NULL, 0);

    fclose(f);
    return ok;
}

void MikuPan_ScreenshotRequest(void)
{
    g_screenshot_pending = 1;
}

void MikuPan_ScreenshotCaptureIfRequested(int drawable_w, int drawable_h)
{
    if (!g_screenshot_pending) return;
    g_screenshot_pending = 0;

    if (drawable_w <= 0 || drawable_h <= 0) return;

    const size_t row_bytes   = (size_t) drawable_w * 4u;
    const size_t pixel_bytes = (size_t) drawable_h * row_bytes;

    unsigned char *rgba = (unsigned char *) malloc(pixel_bytes);
    if (rgba == NULL)
    {
        info_log("Screenshot: malloc failed (%zu bytes)", pixel_bytes);
        return;
    }

    if (!MikuPan_ReadFramebufferRGBA8TopLeft(drawable_w, drawable_h, rgba))
    {
        free(rgba);
        info_log("Screenshot: framebuffer readback failed");
        return;
    }

    char filename[128];
    time_t t = time(NULL);
    struct tm lt;
#if defined(_WIN32)
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    snprintf(filename, sizeof(filename),
             "mikupan_screenshot_%04d%02d%02d_%02d%02d%02d.png",
             lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
             lt.tm_hour, lt.tm_min, lt.tm_sec);

    char path[1024];
    if (!MikuPan_ResolveUserPath(filename, path, sizeof(path)))
    {
        info_log("Screenshot: failed to resolve output path");
        free(rgba);
        return;
    }

    if (MikuPan_ScreenshotWritePng(path, rgba, drawable_w, drawable_h))
    {
        info_log("Screenshot saved: %s (%dx%d)", path, drawable_w, drawable_h);
    }
    else
    {
        info_log("Screenshot: failed to write %s", path);
    }

    free(rgba);
}
