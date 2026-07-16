#ifndef MIKUPAN_FLASHLIGHT_DUST_H
#define MIKUPAN_FLASHLIGHT_DUST_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MikuPan_FlashlightDustSettings
{
    int enabled;
    int ambient_enabled;
    int room_filter_enabled;
    int level;
    int door_disturbance_enabled;
    float door_disturbance_strength;
    float door_disturbance_duration;
} MikuPan_FlashlightDustSettings;

typedef struct MikuPan_FlashlightDustDebugInfo
{
    int enabled;
    int ambient_enabled;
    int room_filter_enabled;
    int door_disturbance_enabled;
    int room_no;
    int room_allowed;
    float room_density;
    int level;
    int ambient_particle_budget;
    int ambient_particle_count;
    int door_particle_count;
    int active_door_bursts;
    int vertex_count;
    int last_door_trigger_valid;
    int last_door_work_index;
    int last_door_id;
    float last_door_position[3];
    float last_door_distance;
    int last_door_target_room_no;
    int last_door_target_room_allowed;
    int last_door_spawned;
} MikuPan_FlashlightDustDebugInfo;

typedef struct MikuPan_FlashlightDustRoomInfo
{
    int room_no;
    const char* name;
    float density;
} MikuPan_FlashlightDustRoomInfo;

MikuPan_FlashlightDustSettings* MikuPan_GetFlashlightDustSettings(void);
const MikuPan_FlashlightDustDebugInfo* MikuPan_GetFlashlightDustDebugInfo(void);
void MikuPan_RenderFlashlightDust(void);
void MikuPan_UpdateDoorDustTriggers(void);
void MikuPan_TriggerDoorDust(float x, float y, float z, float rotation, int room_no);
void MikuPan_TriggerDoorDustAtPlayer(void);
int MikuPan_TriggerDoorDustAtLastDoor(void);
int MikuPan_IsFlashlightDustRoomEnabled(int room_no);
float MikuPan_GetFlashlightDustRoomDensity(int room_no);
int MikuPan_GetFlashlightDustRoomCount(void);
const MikuPan_FlashlightDustRoomInfo* MikuPan_GetFlashlightDustRoomInfo(int index);

#ifdef __cplusplus
}
#endif

#endif
