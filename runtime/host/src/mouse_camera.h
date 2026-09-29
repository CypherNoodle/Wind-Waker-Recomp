#ifndef BLUEWAKE_MOUSE_CAMERA_H
#define BLUEWAKE_MOUSE_CAMERA_H

#include "core/cpu.h"
#include "gxruntime/platform.h"

// Mouse camera (the Mac host): click the game window to hand it the mouse,
// then moving it turns the camera around Link, left and right and up and
// down, and left click is A; Esc, or leaving the window, gives the mouse back. The mouse turns the
// game's own camera (its view angles), so walls, the stick's direction and the
// camera's easing behind Link all carry on as they do.
//
//   BLUEWAKE_MOUSE_CAMERA=0              off
//   BLUEWAKE_MOUSE_SENSITIVITY=1.0       degrees per point of mouse travel, scaled
//   BLUEWAKE_MOUSE_INVERT_Y=1            moving the mouse forward looks down
//   BLUEWAKE_MOUSE_TRACE=1               log the camera's angles

// Once, after the Aurora window exists.
void bluewake_mouse_camera_install(void);
// Once the guest is running (the camera's state is in its memory).
void bluewake_mouse_camera_attach(CPUState* cpu);
// Once per retrace.
void bluewake_mouse_camera_retrace(void);
// At every dispatch boundary (the chassis edge service): at camera_draw's
// entry the frame's camera is final, and the mouse's view is applied.
void bluewake_mouse_camera_dispatch(CPUState* cpu, u32 address);
// On every pad read, on channel 0's live state: left click is A.
void bluewake_mouse_camera_pad(DolPadState* pad);

#endif
