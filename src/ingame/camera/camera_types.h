#ifndef INGAME_CAMERA_CAMERA_TYPES_H
#define INGAME_CAMERA_CAMERA_TYPES_H

#include "typedefs.h"

typedef struct {

	/// Position: Literal location in 3D space "global postion"
	sceVu0FVECTOR p;

	/// Interest: Visual interest, what the camera is facing not a set interest anchor.
	sceVu0FVECTOR i; 
	float roll;
	float fov;
	float nearz;
	float farz;
	float ax;
	float ay;
	float cx;
	float cy;
	float zmin;
	float zmax;
	float pad[2];
	sceVu0FMATRIX vs;
	sceVu0FMATRIX vc;
	sceVu0FMATRIX vcv;
	sceVu0FMATRIX wv;
	sceVu0FMATRIX ws;
	sceVu0FMATRIX wc;
	sceVu0FMATRIX wcv;
	sceVu0FVECTOR zd;
	sceVu0FVECTOR yd;
} SgCAMERA;

typedef struct {
	u_char type;
	u_char id;
	u_short p0[3];
	u_short p1[3];
	u_short p2[3];
	u_short p3[3];
	float roll[2];
	float fov[2];
} MAP_CAM_DAT;

typedef struct {
    u_short no;
    u_short no_old;
    u_char kind;
    u_char type;
    u_char change;
    u_char pad;
    MAP_CAM_DAT *mcd;
} MAP_CAM_INFO;

typedef struct {
	sceVu0FVECTOR i;
	sceVu0FVECTOR p;
	sceVu0FVECTOR rot_y;
	sceVu0FVECTOR rot_x;
} CAM_ID_MOVE;

#define CAMERA_DEBUG_PATH_MAX_POINTS 4

typedef struct {
    u_char active;
    u_char kind;
    u_char type;
    u_char change;
    u_short no;
    u_short no_old;
    u_char camera_path_points;
    u_char interest_path_points;
    sceVu0FVECTOR camera_path[CAMERA_DEBUG_PATH_MAX_POINTS];
    sceVu0FVECTOR interest_path[CAMERA_DEBUG_PATH_MAX_POINTS];
    sceVu0FVECTOR target_p;
    sceVu0FVECTOR target_i;
} CAMERA_DEBUG_PATH;

#endif // INGAME_CAMERA_CAMERA_TYPES_H
