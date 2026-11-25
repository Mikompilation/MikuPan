#ifndef MIKUPAN_MIKUPAN_FINDER_GYRO_H
#define MIKUPAN_MIKUPAN_FINDER_GYRO_H

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Gyro aiming.
 *
 * While the viewfinder is up, a gamepad with a gyroscope aims it by being
 * turned, the way the mouse does: the game asks for it each frame with
 * MikuPan_FinderGyroRequest() and the turn goes straight onto its yaw and
 * pitch, not through the stick.  Nothing else uses the gyro -- walking around
 * with the finder down, in first person or not, ignores it entirely.
 *
 * Every sample SDL reports is integrated over its own sensor timestamp, so the
 * turn is an angle, not a rate: tilting the pad 10 degrees turns the camera 10
 * degrees times the sensitivity however the samples fell between frames.
 *
 * A gyro reads a little off zero when still, and that bias walks with
 * temperature.  It is learned continuously whenever the pad lies still with
 * the finder down -- never while aiming, where a slow, steady turn would look
 * the same -- and MikuPan_FinderGyroRecalibrate() starts it over.  What bias
 * is left is damped by easing rotation slower than a couple of degrees a
 * second toward zero, which is what stops the view creeping.
 */

/// Sensitivity multiplies the controller's own rotation: 1.0 turns the camera
/// as far as the pad turned.
#define MIKUPAN_FINDER_GYRO_SENSITIVITY_DEFAULT (1.5f)
#define MIKUPAN_FINDER_GYRO_SENSITIVITY_MIN     (0.1f)
#define MIKUPAN_FINDER_GYRO_SENSITIVITY_MAX     (8.0f)

/// Which rotation turns the camera left and right. Pitch -- tipping the pad's
/// far edge up or down -- always looks up and down.
#define MIKUPAN_FINDER_GYRO_TURN_YAW  (0) /* point the pad left and right, like a torch */
#define MIKUPAN_FINDER_GYRO_TURN_ROLL (1) /* tip it sideways, like a steering wheel */

/// What the menus show. rate_* is the pad's motion now, bias removed, in
/// degrees a second and in the camera's own signs (yaw positive to the right,
/// pitch positive up), before sensitivity -- so it reads the same whatever the
/// setting. calibration runs 0..1 while the first bias is being learned.
typedef struct
{
    int has_gamepad;
    int has_gyro;  /* the open pad reports one */
    int active;    /* reporting now: enabled in the settings and on the pad */
    int aiming;    /* the game is asking for it */
    float rate_hz; /* the pad's own sample rate, 0 if it does not say */
    float calibration;
    float bias_deg[3]; /* pitch, yaw, roll, degrees a second */
    float rate_yaw_deg;
    float rate_pitch_deg;
} MikuPan_FinderGyroStatus;

/// The gamepad layer opened a pad, or is about to close the one it has (NULL).
/// Switches the pad's sensor to match the setting and starts the calibration
/// over.
void MikuPan_FinderGyroSetGamepad(SDL_Gamepad* gamepad);
/// Feed every event through. Only gyro samples and focus loss are looked at.
void MikuPan_FinderGyroProcessEvent(const SDL_Event* event);
/// Once per game frame, before the game runs: hand over the turn gathered
/// since the last one.
void MikuPan_FinderGyroBeginFrame(void);

/// Say "the finder is up this frame". Must be repeated every frame; it lapses
/// by itself one frame after the requests stop.
void MikuPan_FinderGyroRequest(void);
/// This frame's turn in radians -- yaw positive to the right, pitch positive
/// upward, sensitivity and invert already applied. Returns non-zero when there
/// is any.
int MikuPan_FinderGyroLook(float* yaw, float* pitch);

int MikuPan_FinderGyroEnabled(void);
void MikuPan_SetFinderGyroEnabled(int enabled);
float MikuPan_FinderGyroSensitivity(void);
void MikuPan_SetFinderGyroSensitivity(float sensitivity);
int MikuPan_FinderGyroInvertX(void);
void MikuPan_SetFinderGyroInvertX(int invert);
int MikuPan_FinderGyroInvertY(void);
void MikuPan_SetFinderGyroInvertY(int invert);
int MikuPan_FinderGyroTurnAxis(void);
void MikuPan_SetFinderGyroTurnAxis(int axis);
int MikuPan_FinderGyroSettingsAreDefault(void);
void MikuPan_FinderGyroResetSettings(void);

/// Forget the learned bias; it is learned again the next time the pad lies
/// still with the finder down.
void MikuPan_FinderGyroRecalibrate(void);

void MikuPan_FinderGyroGetStatus(MikuPan_FinderGyroStatus* out);

#ifdef __cplusplus
}
#endif

#endif//MIKUPAN_MIKUPAN_FINDER_GYRO_H
