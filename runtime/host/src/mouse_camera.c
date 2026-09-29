#include "mouse_camera.h"

#include "gxruntime/aurora_backend.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

// The game's camera (GZLE01): dComIfGp_getCamera(0), the camera_process_class
// in g_dComIfG_gameInfo.play.mCameraInfo[0]. Its view (view_class) is at +0,
// with the eye, center and up the frame is drawn from (mLookat) at +0xD8, and
// its dCamera_c at +0x244. The camera's type routine (the follow camera,
// usually) eases dCamera_c::mViewCache every frame and bumpCheck makes the
// final eye from it (walls), which camera_execute copies to the view; then
// camera_draw makes the view matrix from mLookat.
enum {
    kCameraPointer = 0x803CA718u,
    kCameraDraw = 0x8017C350u, // camera_draw__FP20camera_process_class
    kLookatEye = 0xD8u,
    kLookatCenter = 0xE4u,
    kCameraBody = 0x244u,
    kFinalRadius = 0x08u, // dCamera_c::mDirection (cSGlobe: radius, V, U)
    kFinalPitch = 0x0Cu,
    kFinalYaw = 0x0Eu,
    kFinalEye = 0x1Cu,    // dCamera_c::mEye
    kViewRadius = 0x3Cu,  // dCamera_c::mViewCache.mDirection
    kViewPitch = 0x40u,
    kViewYaw = 0x42u,
    kViewCenter = 0x44u,  // mViewCache.mCenter
    kViewEye = 0x50u,     // mViewCache.mEye
    kMode = 0x13Cu,       // dCamera_c::mCurMode
    kEventMode = 0x803C9EA2u, // g_dComIfG_gameInfo.play.mEvtCtrl's mode
    kPlayerPointer = 0x803CA74Cu,
    kPlayerDemoMode = 0x314u,
};

// Degrees a point of pointer travel turns the camera, at sensitivity 1, and
// how far the camera may tilt: from a little below Link, looking up, to high
// above, looking down (the game sits at about 6 degrees above).
static const double kDegreesPerPoint = 0.18;
static const double kPitchMin = -35.0, kPitchMax = 75.0;
static const double kAngleUnits = 65536.0 / 360.0;

static bool g_enabled;
static bool g_captured;
static bool g_click; // left button held while the mouse is the camera: A
static SDL_WindowID g_window;
static double g_sum_x, g_sum_y;
static double g_sensitivity = 1.0;
static double g_invert_y = 1.0;
// The follow camera steers the view itself every frame (it eases the tilt back
// to its own and the yaw toward behind Link), and walls and its smoothing move
// the final eye after that. So from the mouse's first move the mouse owns the
// view's angles until it is let go or the game takes the camera (a cutscene, a
// door, Z-targeting, first person): each frame is drawn at exactly the angles
// the mouse set, at the distance the game chose.
static bool g_held;
static double g_pitch, g_yaw;
static bool g_trace;
static unsigned long long g_retrace;

// BLUEWAKE_MOUSE_TEST=retrace:dx:dy:length,...: pointer motion per retrace,
// for testing without a mouse.
typedef struct {
    unsigned long long start, length;
    double dx, dy;
} TestMove;
static TestMove g_test[16];
static unsigned g_test_count;

static void set_captured(bool captured) {
    SDL_Window* window = g_window != 0 ? SDL_GetWindowFromID(g_window) : NULL;
    if (window == NULL || captured == g_captured)
        return;
    if (!SDL_SetWindowRelativeMouseMode(window, captured))
        return;
    g_captured = captured;
    g_click = false;
    g_sum_x = g_sum_y = 0.0;
    g_held = false;
    fprintf(stderr, "[mouse] camera %s\n",
            captured ? "on (left click is A, Esc gives the mouse back)" : "off (click to turn it on)");
}

static void observe(const void* sdl_event, void* user) {
    (void)user;
    const SDL_Event* event = (const SDL_Event*)sdl_event;
    switch (event->type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button != SDL_BUTTON_LEFT)
            break;
        if (g_captured) {
            g_click = true;
        } else {
            // The click that hands over the mouse is not a press.
            g_window = event->button.windowID;
            set_captured(true);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button == SDL_BUTTON_LEFT)
            g_click = false;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (g_captured) {
            g_sum_x += event->motion.xrel;
            g_sum_y += event->motion.yrel;
        }
        break;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.scancode == SDL_SCANCODE_ESCAPE)
            set_captured(false);
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_MINIMIZED:
        set_captured(false);
        break;
    default:
        break;
    }
}

void bluewake_mouse_camera_install(void) {
#if defined(__APPLE__) && TARGET_OS_IPHONE
    // An iPad's touches arrive as mouse events too; its controls are on screen.
    return;
#else
    const char* on = getenv("BLUEWAKE_MOUSE_CAMERA");
    if (on != NULL && on[0] == '0')
        return;
    g_enabled = true;
    dol_aurora_set_event_observer(observe, NULL);
    fprintf(stderr, "[mouse] click the game to turn the camera with the mouse\n");
#endif
}

void bluewake_mouse_camera_attach(CPUState* cpu) {
    (void)cpu;
    const char* sensitivity = getenv("BLUEWAKE_MOUSE_SENSITIVITY");
    if (sensitivity != NULL && atof(sensitivity) > 0.0)
        g_sensitivity = atof(sensitivity);
    const char* invert = getenv("BLUEWAKE_MOUSE_INVERT_Y");
    if (invert != NULL && invert[0] == '1')
        g_invert_y = -1.0;
    const char* trace = getenv("BLUEWAKE_MOUSE_TRACE");
    g_trace = trace != NULL && trace[0] == '1';
    const char* test = getenv("BLUEWAKE_MOUSE_TEST");
    for (const char* p = test; p != NULL && *p != '\0' && g_test_count < 16u;) {
        TestMove move;
        int used = 0;
        if (sscanf(p, "%llu:%lf:%lf:%llu%n", &move.start, &move.dx, &move.dy, &move.length, &used) != 4)
            break;
        g_test[g_test_count++] = move;
        p += used;
        if (*p == ',')
            ++p;
    }
}

void bluewake_mouse_camera_pad(DolPadState* pad) {
    if (g_click)
        pad->button |= 0x0100u; // PAD_BUTTON_A
}

static float read_f32(CPUState* cpu, u32 address) {
    const u32 bits = mem_read32(cpu, address);
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static void write_f32(CPUState* cpu, u32 address, float value) {
    u32 bits;
    memcpy(&bits, &value, sizeof bits);
    mem_write32(cpu, address, bits);
}

static bool guest_pointer(u32 address) {
    return address >= 0x80000000u && address < 0x81800000u;
}

// The player has the camera: the follow camera's mode, no event or cutscene.
static bool camera_free(CPUState* cpu, u32 camera) {
    if (mem_read32(cpu, camera + kMode) != 0u || mem_read8(cpu, kEventMode) != 0u)
        return false;
    const u32 player = mem_read32(cpu, kPlayerPointer);
    return guest_pointer(player) && mem_read32(cpu, player + kPlayerDemoMode) == 0u;
}

// center + radius along the globe's angles (cSGlobe::Xyz: R cos V sin U,
// R sin V, R cos V cos U), into `eye`.
static void place_eye(CPUState* cpu, u32 center, u32 eye, double radius, double pitch, double yaw) {
    const double v = pitch * M_PI / 180.0, u = yaw * M_PI / 180.0;
    write_f32(cpu, eye, (float)(read_f32(cpu, center) + radius * cos(v) * sin(u)));
    write_f32(cpu, eye + 4u, (float)(read_f32(cpu, center + 4u) + radius * sin(v)));
    write_f32(cpu, eye + 8u, (float)(read_f32(cpu, center + 8u) + radius * cos(v) * cos(u)));
}

static float distance(CPUState* cpu, u32 a, u32 b) {
    const float dx = read_f32(cpu, a) - read_f32(cpu, b), dy = read_f32(cpu, a + 4u) - read_f32(cpu, b + 4u),
                dz = read_f32(cpu, a + 8u) - read_f32(cpu, b + 8u);
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// At camera_draw's entry: this frame's camera is final and about to be drawn.
static void camera_frame(CPUState* cpu, u32 process) {
    const u32 camera = process + kCameraBody;
    if (!camera_free(cpu, camera)) {
        // A cutscene or a special camera keeps its own view.
        g_sum_x = g_sum_y = 0.0;
        g_held = false;
        return;
    }
    if (g_sum_x == 0.0 && g_sum_y == 0.0 && !g_held)
        return;
    if (!g_held) {
        g_pitch = (s16)mem_read16(cpu, camera + kViewPitch) / kAngleUnits;
        g_yaw = (s16)mem_read16(cpu, camera + kViewYaw) / kAngleUnits;
        g_held = true;
    }
    // Pointer right turns the view right (the camera swings the other way
    // round Link); pointer forward looks up (the camera drops).
    const double scale = kDegreesPerPoint * g_sensitivity;
    g_yaw = fmod(g_yaw - g_sum_x * scale, 360.0);
    g_pitch += g_sum_y * scale * g_invert_y;
    g_pitch = g_pitch < kPitchMin ? kPitchMin : g_pitch > kPitchMax ? kPitchMax : g_pitch;
    g_sum_x = g_sum_y = 0.0;
    const s16 v = (s16)lrint(g_pitch * kAngleUnits), u = (s16)lrint(g_yaw * kAngleUnits);

    // Next frame's camera starts from these angles...
    const float radius = read_f32(cpu, camera + kViewRadius);
    if (radius > 1.0f && radius < 100000.0f) {
        mem_write16(cpu, camera + kViewPitch, (u16)v);
        mem_write16(cpu, camera + kViewYaw, (u16)u);
        place_eye(cpu, camera + kViewCenter, camera + kViewEye, radius, g_pitch, g_yaw);
    }
    // ... and this frame is drawn at them, at the distance the game chose
    // (walls push it in).
    const u32 center = process + kLookatCenter, eye = process + kLookatEye;
    const float reach = distance(cpu, eye, center);
    if (reach > 1.0f && reach < 100000.0f) {
        place_eye(cpu, center, eye, reach, g_pitch, g_yaw);
        mem_write16(cpu, camera + kFinalPitch, (u16)v);
        mem_write16(cpu, camera + kFinalYaw, (u16)u);
        place_eye(cpu, camera + kViewCenter, camera + kFinalEye, read_f32(cpu, camera + kFinalRadius), g_pitch,
                  g_yaw);
    }
}

void bluewake_mouse_camera_dispatch(CPUState* cpu, u32 address) {
    if (address != kCameraDraw || cpu == NULL)
        return;
    const u32 process = cpu->gpr[3];
    if (process != mem_read32(cpu, kCameraPointer) || !guest_pointer(process))
        return;
    camera_frame(cpu, process);
    if (g_trace) {
        const u32 camera = process + kCameraBody;
        const u32 player = mem_read32(cpu, kPlayerPointer);
        fprintf(stderr,
                "[mouse-trace] retrace=%llu mode=%u event=%u demo=%u view V=%d U=%d final V=%d U=%d reach=%.1f held=%d\n",
                g_retrace, mem_read32(cpu, camera + kMode), mem_read8(cpu, kEventMode),
                guest_pointer(player) ? mem_read32(cpu, player + kPlayerDemoMode) : 99u,
                (s16)mem_read16(cpu, camera + kViewPitch),
                (s16)mem_read16(cpu, camera + kViewYaw), (s16)mem_read16(cpu, camera + kFinalPitch),
                (s16)mem_read16(cpu, camera + kFinalYaw),
                distance(cpu, process + kLookatEye, process + kLookatCenter), g_held ? 1 : 0);
    }
}

void bluewake_mouse_camera_retrace(void) {
    ++g_retrace;
    for (unsigned i = 0; i < g_test_count; ++i) {
        if (g_retrace >= g_test[i].start && g_retrace < g_test[i].start + g_test[i].length) {
            g_sum_x += g_test[i].dx;
            g_sum_y += g_test[i].dy;
        }
    }
}
