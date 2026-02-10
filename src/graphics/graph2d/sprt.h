#ifndef GRAPHICS_GRAPH2D_SPRT_H
#define GRAPHICS_GRAPH2D_SPRT_H

#include "typedefs.h"

typedef struct {
	u_long tex0;
    /// Shifts the image content within the texture's frame on the x
	u_short u;

    /// Shifts the image content within the texture's frame on the y
	u_short v;
	u_short w; /// Width of the frame for the texture
	u_short h; /// Height of the frame for the texture
	int x; /// Absolute X coordinate of the texture's (upper-left corner) on the screen
	int y; /// Absolute Y coordinate of the texture's (upper-left corner) on the screen
	int pri;
	u_char alpha;
} SPRT_DAT;

typedef struct {
	short int u; /// Shifts the image content within the texture's frame on the x
	short int v; /// Shifts the image content within the texture's frame on the y
	short int w; /// Width of the frame for the texture
	short int h; /// Height of the frame for the texture
	short int x; /// Absolute X coordinate of the texture's (upper-left corner) on the screen
	short int y; /// Absolute Y coordinate of the texture's (upper-left corner) on the screen
	u_char pri;
	u_char alp;
} SPRT_SDAT;

typedef struct {
	short int cx;
	short int cy;
	float rot;
} SPRT_SROT;

typedef struct {
	short int cx;
	short int cy;
	short int sw;
	short int sh;
} SPRT_SSCL;

typedef struct {
	u_int att;

    /// Shifts the image content within the texture's frame on the x
	u_int u;

    /// Shifts the image content within the texture's frame on the y
	u_int v;

    /// Width of the frame for the texture
	u_int w;

    /// Height of the frame for the texture
	u_int h;
	float crx;
	float cry;
	float csx;
	float csy;

    /// Absolute X coordinate of the texture's (upper-left corner) on the screen
	float x;

    /// Absolute Y coordinate of the texture's (upper-left corner) on the screen
	float y;
	u_int z;

    /// Frame's width multiplier
	float scw;

    /// Frame's height multiplier
	float sch;

    /// Rotation
	float rot;
	u_long gftg;
	u_long tex0;
	u_long tex1;
	u_long texa;
	u_long alphar;
	u_long zbuf;
	u_long test;
	u_int pri;
	u_char r;
	u_char g;
	u_char b;
	u_char alpha;
	u_char col;
} DISP_SPRT;

typedef struct {
    u_int att;
    float crx;
    float cry;
    float csx;
    float csy;
    int x[4];
    int y[4];
    u_int z;
    float scw;
    float sch;
    float rot;
    u_long texa;
    u_long alphar;
    u_long zbuf;
    u_long test;
    u_int pri;
    u_char r[4];
    u_char g[4];
    u_char b[4];
    u_char alpha;
} DISP_SQAR;

typedef struct {
    u_int w;
    u_int h;
    int x;
    int y;
    u_int pri;
    u_char r;
    u_char g;
    u_char b;
    u_char alpha;
} SQAR_DAT;

typedef struct {
    u_int w;
    u_int h;
    int x;
    int y;
    u_int pri;
    u_char r[4];
    u_char g[4];
    u_char b[4];
    u_char alpha;
} GSQR_DAT;

typedef struct {
    int x[4];
    int y[4];
    u_int pri;
    u_char r;
    u_char g;
    u_char b;
    u_char alpha;
} SQR4_DAT;

typedef struct {
    int x[4];
    int y[4];
    u_int pri;
    u_char r[4];
    u_char g[4];
    u_char b[4];
    u_char alpha;
} GSQ4_DAT;

#endif // GRAPHICS_GRAPH2D_SPRT_H
