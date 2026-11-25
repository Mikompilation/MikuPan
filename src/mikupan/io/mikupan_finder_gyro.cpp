#include "mikupan_finder_gyro.h"

#include "mikupan/debug/mikupan_logging_c.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/ui/mikupan_rml_options.h"
#include "mikupan/ui/mikupan_ui.h"

#include <SDL3/SDL_stdinc.h>

#include <cstring>

namespace
{
constexpr float DegToRad(float degrees)
{
    return degrees * (SDL_PI_F / 180.0f);
}

constexpr float kRadToDeg = 180.0f / SDL_PI_F;

/* Rotation slower than this is eased toward zero (out = rate * rate / this):
 * the fraction of a degree a second a gyro reads after calibration cannot
 * creep the view, and a deliberate turn passes untouched. */
constexpr float kTightenRad = DegToRad(2.0f);

/* "Still", for calibration: every axis within the noise of its own short
 * average, and that average under the cap -- no gyro's bias is anywhere near
 * ten degrees a second, so anything faster is the pad moving, however
 * smoothly.  There is deliberately no test against the current bias estimate:
 * a wrong estimate would then refuse the very samples that correct it. */
constexpr float kStillNoiseRad = DegToRad(2.0f);
constexpr float kStillCapRad = DegToRad(10.0f);
constexpr float kFastTauS = 0.2f;    /* the short average */
constexpr float kSettleS = 0.5f;     /* still this long before learning */
constexpr float kFirstTauS = 0.25f;  /* learning rate until calibrated... */
constexpr float kBiasTauS = 1.0f;    /* ...and after */
constexpr float kCalibratedS = 1.0f; /* learning this long counts as done */

/* A gap longer than this between two samples is a pad waking up or the window
 * coming back, not a turn; the sample is weighted at the nominal rate
 * instead. */
constexpr float kMaxDtS = 0.05f;
constexpr float kFallbackHz = 250.0f;

/* The open pad: whether it has a gyro, whether it is reporting, its rate. */
SDL_Gamepad* s_gamepad = nullptr;
bool s_has_gyro = false;
bool s_active = false;
float s_rate_hz = 0.0f;
bool s_enable_failure_logged = false;

/* The game's requests, for the frame now running and the one before.  Both
 * count, so aiming neither drops in the gap between a frame starting and the
 * game asking again, nor outlives the finder by more than a frame. */
bool s_request = false;
bool s_request_prev = false;
/* A menu is over the game and owns the pad. */
bool s_suppressed = false;

/* Sensor timestamp of the last sample, 0 for none since the pad (re)started. */
Uint64 s_last_ns = 0;

/* Calibration, all in radians a second, SDL's axes (pitch, yaw, roll). */
float s_fast[3] = {};
bool s_fast_valid = false;
float s_bias[3] = {};
float s_still_s = 0.0f;
float s_calibrated_s = 0.0f;

/* The last sample, bias removed, in the camera's signs -- for the readout. */
float s_rate_yaw = 0.0f;
float s_rate_pitch = 0.0f;

/* Turn gathered since the last frame, and the last frame's share of it, in
 * radians in the camera's signs (yaw right, pitch up), before sensitivity. */
float s_motion_yaw = 0.0f;
float s_motion_pitch = 0.0f;
float s_look_yaw = 0.0f;
float s_look_pitch = 0.0f;

MikuPan_ConfigFinderGyro& Settings(void)
{
    return mikupan_configuration.input.finder_gyro;
}

bool Aiming(void)
{
    return s_request || s_request_prev;
}

/* Drop motion in flight and the sample clock, keeping the calibration.  For
 * anything that breaks the run of samples: focus lost, the gyro switched. */
void ClearMotion(void)
{
    s_last_ns = 0;
    s_motion_yaw = 0.0f;
    s_motion_pitch = 0.0f;
    s_look_yaw = 0.0f;
    s_look_pitch = 0.0f;
    s_rate_yaw = 0.0f;
    s_rate_pitch = 0.0f;
    s_still_s = 0.0f;
    s_fast_valid = false;
}

/* Forget the bias as well.  A different pad has a different one. */
void ResetCalibration(void)
{
    ClearMotion();
    std::memset(s_bias, 0, sizeof(s_bias));
    s_calibrated_s = 0.0f;
}

const char* PadName(void)
{
    const char* name = SDL_GetGamepadName(s_gamepad);
    return name != nullptr && name[0] != '\0' ? name : "<unknown controller>";
}

/* Bring the open pad's gyro in line with the setting: on when there is a pad,
 * it has a gyro and gyro aiming is on, off otherwise.  Called when a pad opens
 * and when the setting changes; harmless to call again. */
void SyncSensor(void)
{
    if (s_gamepad == nullptr)
    {
        s_has_gyro = false;
        s_active = false;
        s_rate_hz = 0.0f;
        ClearMotion();
        return;
    }

    s_has_gyro = SDL_GamepadHasSensor(s_gamepad, SDL_SENSOR_GYRO);
    bool want = s_has_gyro && Settings().enabled != 0;

    if (s_has_gyro && !SDL_SetGamepadSensorEnabled(s_gamepad, SDL_SENSOR_GYRO, want))
    {
        if (!s_enable_failure_logged)
        {
            s_enable_failure_logged = true;
            info_log("Could not %s the gyro on %s: %s", want ? "enable" : "disable",
                     PadName(), SDL_GetError());
        }
        want = false;
    }

    if (want != s_active)
    {
        ClearMotion();
    }
    s_active = want;
    s_rate_hz = s_has_gyro ? SDL_GetGamepadSensorDataRate(s_gamepad, SDL_SENSOR_GYRO)
                           : 0.0f;
}

/* One gyro sample: learn the bias when the pad is still and nobody is aiming,
 * and while the game is aiming add the turn to this frame's motion. */
void Sample(const float* w, Uint64 sample_ns)
{
    if (!s_active)
    {
        return;
    }

    float dt = 0.0f;
    if (s_last_ns != 0 && sample_ns > s_last_ns)
    {
        dt = static_cast<float>(static_cast<double>(sample_ns - s_last_ns) * 1.0e-9);
    }
    s_last_ns = sample_ns;
    if (!(dt > 0.0f) || dt > kMaxDtS)
    {
        dt = 1.0f / (s_rate_hz > 0.0f ? s_rate_hz : kFallbackHz);
    }

    /* Calibration. */
    if (!s_fast_valid)
    {
        std::memcpy(s_fast, w, sizeof(s_fast));
        s_fast_valid = true;
    }
    else
    {
        const float k = dt / (kFastTauS + dt);
        for (int i = 0; i < 3; i++)
        {
            s_fast[i] += (w[i] - s_fast[i]) * k;
        }
    }

    bool still = true;
    for (int i = 0; i < 3; i++)
    {
        if (SDL_fabsf(w[i] - s_fast[i]) > kStillNoiseRad
            || SDL_fabsf(s_fast[i]) > kStillCapRad)
        {
            still = false;
        }
    }

    /* Never while aiming: a slow, steady turn through the finder is exactly
     * what a still pad with a bias looks like. */
    if (still && !Aiming())
    {
        s_still_s += dt;
        if (s_still_s >= kSettleS)
        {
            const float tau = s_calibrated_s < kCalibratedS ? kFirstTauS : kBiasTauS;
            const float k = dt / (tau + dt);
            for (int i = 0; i < 3; i++)
            {
                s_bias[i] += (s_fast[i] - s_bias[i]) * k;
            }
            if (s_calibrated_s < kCalibratedS)
            {
                s_calibrated_s += dt;
            }
        }
    }
    else
    {
        s_still_s = 0.0f;
    }

    float r[3];
    for (int i = 0; i < 3; i++)
    {
        r[i] = w[i] - s_bias[i];
    }

    /* SDL's rotations are counter-clockwise positive seen from the axis's own
     * end: positive yaw points the pad left, positive roll leans its top left,
     * positive pitch lifts its far edge.  The camera turns right on positive
     * yaw and looks up on positive pitch, so the turn axis is negated. */
    const float turn =
        Settings().turn_axis == MIKUPAN_FINDER_GYRO_TURN_ROLL ? r[2] : r[1];
    float yaw = -turn;
    float pitch = r[0];
    s_rate_yaw = yaw;
    s_rate_pitch = pitch;

    const float speed = SDL_sqrtf(yaw * yaw + pitch * pitch);
    if (speed < kTightenRad)
    {
        const float k = speed / kTightenRad;
        yaw *= k;
        pitch *= k;
    }

    if (Aiming() && !s_suppressed)
    {
        s_motion_yaw += yaw * dt;
        s_motion_pitch += pitch * dt;
    }
}
}// namespace

extern "C" {

void MikuPan_FinderGyroSetGamepad(SDL_Gamepad* gamepad)
{
    /* ImGui holds every pad open, so the one being left would go on reporting
     * after the game has closed it. */
    if (s_gamepad != nullptr && s_active)
    {
        SDL_SetGamepadSensorEnabled(s_gamepad, SDL_SENSOR_GYRO, false);
    }
    s_gamepad = gamepad;

    if (gamepad != nullptr)
    {
        ResetCalibration();
    }
    SyncSensor();
    if (gamepad == nullptr)
    {
        return;
    }

    if (s_has_gyro)
    {
        info_log("%s has a gyro (%.0f Hz); gyro aiming is %s", PadName(),
                 static_cast<double>(s_rate_hz), s_active ? "on" : "off");
    }
    else
    {
        info_log("%s reports no gyro", PadName());
    }
}

void MikuPan_FinderGyroProcessEvent(const SDL_Event* event)
{
    if (event == nullptr)
    {
        return;
    }

    if (event->type == SDL_EVENT_GAMEPAD_SENSOR_UPDATE)
    {
        if (event->gsensor.sensor == SDL_SENSOR_GYRO && s_gamepad != nullptr
            && event->gsensor.which == SDL_GetGamepadID(s_gamepad))
        {
            /* The pad's own clock when it has one; SDL's arrival time is
             * jittered by USB and by how events were batched. */
            Sample(event->gsensor.data, event->gsensor.sensor_timestamp != 0
                                            ? event->gsensor.sensor_timestamp
                                            : event->gsensor.timestamp);
        }
    }
    else if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST)
    {
        /* SDL stops delivering pad events to a window in the background, so
         * the next sample after it comes back is no continuation of this
         * one. */
        ClearMotion();
    }
}

void MikuPan_FinderGyroBeginFrame(void)
{
    s_request_prev = s_request;
    s_request = false;
    s_suppressed = MikuPan_UiIsMenuOpen() || MikuPan_RmlOptionsIsOpen();

    s_look_yaw = s_request_prev ? s_motion_yaw : 0.0f;
    s_look_pitch = s_request_prev ? s_motion_pitch : 0.0f;
    s_motion_yaw = 0.0f;
    s_motion_pitch = 0.0f;
}

void MikuPan_FinderGyroRequest(void)
{
    s_request = true;
}

int MikuPan_FinderGyroLook(float* yaw, float* pitch)
{
    float x = 0.0f;
    float y = 0.0f;

    if (s_active && !s_suppressed)
    {
        const MikuPan_ConfigFinderGyro& settings = Settings();
        x = s_look_yaw * settings.sensitivity;
        y = s_look_pitch * settings.sensitivity;

        if (settings.invert_x != 0)
        {
            x = -x;
        }
        if (settings.invert_y != 0)
        {
            y = -y;
        }
    }

    if (yaw != nullptr)
    {
        *yaw = x;
    }
    if (pitch != nullptr)
    {
        *pitch = y;
    }
    return x != 0.0f || y != 0.0f;
}

int MikuPan_FinderGyroEnabled(void)
{
    return Settings().enabled;
}

void MikuPan_SetFinderGyroEnabled(int enabled)
{
    enabled = enabled ? 1 : 0;
    if (enabled == Settings().enabled)
    {
        return;
    }
    Settings().enabled = enabled;

    /* Switch the pad's sensor with it, so a pad whose gyro is not wanted is
     * not streaming hundreds of reports a second for nothing. */
    SyncSensor();
}

float MikuPan_FinderGyroSensitivity(void)
{
    return Settings().sensitivity;
}

void MikuPan_SetFinderGyroSensitivity(float sensitivity)
{
    /* Written so a NaN lands on the floor too. */
    if (!(sensitivity >= MIKUPAN_FINDER_GYRO_SENSITIVITY_MIN))
    {
        sensitivity = MIKUPAN_FINDER_GYRO_SENSITIVITY_MIN;
    }
    else if (sensitivity > MIKUPAN_FINDER_GYRO_SENSITIVITY_MAX)
    {
        sensitivity = MIKUPAN_FINDER_GYRO_SENSITIVITY_MAX;
    }
    Settings().sensitivity = sensitivity;
}

int MikuPan_FinderGyroInvertX(void)
{
    return Settings().invert_x;
}

void MikuPan_SetFinderGyroInvertX(int invert)
{
    Settings().invert_x = invert ? 1 : 0;
}

int MikuPan_FinderGyroInvertY(void)
{
    return Settings().invert_y;
}

void MikuPan_SetFinderGyroInvertY(int invert)
{
    Settings().invert_y = invert ? 1 : 0;
}

int MikuPan_FinderGyroTurnAxis(void)
{
    return Settings().turn_axis;
}

void MikuPan_SetFinderGyroTurnAxis(int axis)
{
    Settings().turn_axis = axis == MIKUPAN_FINDER_GYRO_TURN_ROLL
                               ? MIKUPAN_FINDER_GYRO_TURN_ROLL
                               : MIKUPAN_FINDER_GYRO_TURN_YAW;
}

int MikuPan_FinderGyroSettingsAreDefault(void)
{
    const MikuPan_ConfigFinderGyro& settings = Settings();
    return settings.enabled == 1
           && settings.sensitivity == MIKUPAN_FINDER_GYRO_SENSITIVITY_DEFAULT
           && settings.invert_x == 0 && settings.invert_y == 0
           && settings.turn_axis == MIKUPAN_FINDER_GYRO_TURN_YAW;
}

void MikuPan_FinderGyroResetSettings(void)
{
    MikuPan_SetFinderGyroSensitivity(MIKUPAN_FINDER_GYRO_SENSITIVITY_DEFAULT);
    MikuPan_SetFinderGyroInvertX(0);
    MikuPan_SetFinderGyroInvertY(0);
    MikuPan_SetFinderGyroTurnAxis(MIKUPAN_FINDER_GYRO_TURN_YAW);
    MikuPan_SetFinderGyroEnabled(1);
}

void MikuPan_FinderGyroRecalibrate(void)
{
    ResetCalibration();
    info_log("Gyro calibration restarted");
}

void MikuPan_FinderGyroGetStatus(MikuPan_FinderGyroStatus* out)
{
    if (out == nullptr)
    {
        return;
    }

    out->has_gamepad = s_gamepad != nullptr;
    out->has_gyro = s_has_gyro;
    out->active = s_active;
    out->aiming = Aiming();
    out->rate_hz = s_rate_hz;
    out->calibration =
        s_calibrated_s >= kCalibratedS ? 1.0f : s_calibrated_s / kCalibratedS;
    for (int i = 0; i < 3; i++)
    {
        out->bias_deg[i] = s_bias[i] * kRadToDeg;
    }
    out->rate_yaw_deg = s_rate_yaw * kRadToDeg;
    out->rate_pitch_deg = s_rate_pitch * kRadToDeg;
}

}
