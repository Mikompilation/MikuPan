#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MovVidHandle_s* MovVidHandle;

MovVidHandle movvid_open(const char* path, const char** err);
void movvid_close(MovVidHandle h);

int movvid_decode_frame(
    MovVidHandle h,
    void* unused,
    int unused_size,
    double* pts,
    const char** err
);

const unsigned char* movvid_rgba_ptr(
    MovVidHandle h,
    int* strideBytes
);

int movvid_w(MovVidHandle h);
int movvid_h(MovVidHandle h);
double movvid_fps(MovVidHandle h);

#ifdef __cplusplus
}
#endif