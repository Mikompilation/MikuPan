#ifndef MIKUPAN_MIKUPAN_SCREENSHOT_H
#define MIKUPAN_MIKUPAN_SCREENSHOT_H

/// Schedule a screenshot to be captured at the next renderer post-process flip.
/// Cheap and idempotent — extra requests within the same frame collapse.
void MikuPan_ScreenshotRequest(void);

/// Called from MikuPan_EndFrame at the moment the post-processed scene has
/// just been drawn to FBO 0 and before the debug UI is composited. Reads the
/// default framebuffer if a request is pending and writes a timestamped PNG
/// to the current working directory. `drawable_w`/`drawable_h` must be the
/// GL drawable size in pixels for the bound framebuffer.
void MikuPan_ScreenshotCaptureIfRequested(int drawable_w, int drawable_h);

/// Write top-down RGBA8 pixels (no row padding) to `path` as an uncompressed
/// opaque RGB PNG. The input alpha channel is discarded so image viewers do not
/// composite screenshots against their own backgrounds. Returns 1 on success,
/// 0 on any I/O failure.
int MikuPan_ScreenshotWritePng(const char *path,
                               const unsigned char *rgba,
                               int width, int height);
int MikuPan_ScreenshotWritePngRgba(const char *path,
                                   const unsigned char *rgba,
                                   int width, int height);

#endif //MIKUPAN_MIKUPAN_SCREENSHOT_H
