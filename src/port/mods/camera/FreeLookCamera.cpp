#include "FreeLookCamera.h"
#include "sm64.h"

extern "C" {
#include "puppycam.h"
#include "game/camera.h"
#include "game/game_init.h"
#include "game/object_list_processor.h"
#include "engine/math_util.h"
#include "game/level_update.h"

extern void mode_behind_mario_camera(struct Camera* c);
extern void mode_cannon_camera(struct Camera* c);
extern void mode_c_up_camera(struct Camera* c);
extern void mode_water_surface_camera(struct Camera* c);
extern void mode_8_directions_camera(struct Camera* c);
extern void mode_radial_camera(struct Camera* c);
extern void mode_outward_radial_camera(struct Camera* c);
extern void mode_lakitu_camera(struct Camera* c);
extern void mode_boss_fight_camera(struct Camera* c);
extern void mode_parallel_tracking_camera(struct Camera* c);
extern void mode_slide_camera(struct Camera* c);
extern void mode_fixed_camera(struct Camera* c);
extern void mode_spiral_stairs_camera(struct Camera* c);
extern void mode_mario_camera(struct Camera* c);

extern s16 sYawSpeed;
extern s16 sStatusFlags;
extern struct PlayerCameraState *sMarioCamState;
extern struct PlayerGeometry sMarioGeometry;
extern void update_lakitu(struct Camera *c);
extern u8 sFramesSinceCutsceneEnded;
}

void FreeLookCameraBoot(void) {
    puppycam_boot();
}

void FreeLookCameraInit(struct Camera* c) {
    // TODO!
    puppycam_init();
}

void FreeLookCameraUpdate(struct Camera* c) {
    // TODO!
    UNUSED u8 unused[24];

    gPuppyCam.enabled = true;
    gPuppyCam.options.analogue = true;
    
    gCamera = c;
    update_camera_hud_status(c);
    if (c->cutscene == 0 && !gPuppyCam.enabled) {
        // Only process R_TRIG if 'fixed' is not selected in the menu
        if (cam_select_alt_mode(0) == CAM_SELECTION_MARIO) {
            if (gPlayer1Controller->buttonPressed & R_TRIG) {
                if (set_cam_angle(0) == CAM_ANGLE_LAKITU) {
                    set_cam_angle(CAM_ANGLE_MARIO);
                } else {
                    set_cam_angle(CAM_ANGLE_LAKITU);
                }
            }
        }
        play_sound_if_cam_switched_to_lakitu_or_mario();
    }

    // Initialize the camera
    sStatusFlags &= ~CAM_FLAG_FRAME_AFTER_CAM_INIT;
    if (gCameraMovementFlags & CAM_MOVE_INIT_CAMERA) {
        init_camera(c);
        gCameraMovementFlags &= ~CAM_MOVE_INIT_CAMERA;
        sStatusFlags |= CAM_FLAG_FRAME_AFTER_CAM_INIT;
    }

    if (!gPuppyCam.enabled || c->cutscene != 0)
    {
    // Store previous geometry information
    sMarioGeometry.prevFloorHeight = sMarioGeometry.currFloorHeight;
    sMarioGeometry.prevCeilHeight = sMarioGeometry.currCeilHeight;
    sMarioGeometry.prevFloor = sMarioGeometry.currFloor;
    sMarioGeometry.prevCeil = sMarioGeometry.currCeil;
    sMarioGeometry.prevFloorType = sMarioGeometry.currFloorType;
    sMarioGeometry.prevCeilType = sMarioGeometry.currCeilType;

    find_mario_floor_and_ceil(&sMarioGeometry);
    gCheckingSurfaceCollisionsForCamera = TRUE;
    vec3f_copy(c->pos, gLakituState.goalPos);
    vec3f_copy(c->focus, gLakituState.goalFocus);

    c->yaw = gLakituState.yaw;
    c->nextYaw = gLakituState.nextYaw;
    c->mode = gLakituState.mode;
    c->defMode = gLakituState.defMode;

    camera_course_processing(c);
    stub_camera_3(c);
    sCButtonsPressed = find_c_buttons_pressed(sCButtonsPressed, gPlayer1Controller->buttonPressed,
                                              gPlayer1Controller->buttonDown);

    if (c->cutscene != 0) {
        sYawSpeed = 0;
        play_cutscene(c);
        sFramesSinceCutsceneEnded = 0;
    } else {
        // Clear the recent cutscene after 8 frames
        if (gRecentCutscene != 0 && sFramesSinceCutsceneEnded < 8) {
            sFramesSinceCutsceneEnded++;
            if (sFramesSinceCutsceneEnded >= 8) {
                gRecentCutscene = 0;
                sFramesSinceCutsceneEnded = 0;
            }
        }
    }
    // If not in a cutscene, do mode processing
    if (c->cutscene == 0) {
        sYawSpeed = 0x400;

        if (sSelectionFlags & CAM_MODE_MARIO_ACTIVE) {
            switch (c->mode) {
                case CAMERA_MODE_BEHIND_MARIO:
                    mode_behind_mario_camera(c);
                    break;

                case CAMERA_MODE_C_UP:
                    mode_c_up_camera(c);
                    break;

                case CAMERA_MODE_WATER_SURFACE:
                    mode_water_surface_camera(c);
                    break;

                case CAMERA_MODE_INSIDE_CANNON:
                    mode_cannon_camera(c);
                    break;

                default:
                    mode_mario_camera(c);
            }
        } else {
            switch (c->mode) {
                case CAMERA_MODE_BEHIND_MARIO:
                    mode_behind_mario_camera(c);
                    break;

                case CAMERA_MODE_C_UP:
                    mode_c_up_camera(c);
                    break;

                case CAMERA_MODE_WATER_SURFACE:
                    mode_water_surface_camera(c);
                    break;

                case CAMERA_MODE_INSIDE_CANNON:
                    mode_cannon_camera(c);
                    break;

                case CAMERA_MODE_8_DIRECTIONS:
                    mode_8_directions_camera(c);
                    break;

                case CAMERA_MODE_RADIAL:
                    mode_radial_camera(c);
                    break;

                case CAMERA_MODE_OUTWARD_RADIAL:
                    mode_outward_radial_camera(c);
                    break;

                case CAMERA_MODE_CLOSE:
                    mode_lakitu_camera(c);
                    break;

                case CAMERA_MODE_FREE_ROAM:
                    mode_lakitu_camera(c);
                    break;
                case CAMERA_MODE_BOSS_FIGHT:
                    mode_boss_fight_camera(c);
                    break;

                case CAMERA_MODE_PARALLEL_TRACKING:
                    mode_parallel_tracking_camera(c);
                    break;

                case CAMERA_MODE_SLIDE_HOOT:
                    mode_slide_camera(c);
                    break;

                case CAMERA_MODE_FIXED:
                    mode_fixed_camera(c);
                    break;

                case CAMERA_MODE_SPIRAL_STAIRS:
                    mode_spiral_stairs_camera(c);
                    break;
            }
        }
    }
    }
    // Start any Mario-related cutscenes
    start_cutscene(c, get_cutscene_from_mario_status(c));
    stub_camera_2(c);
    gCheckingSurfaceCollisionsForCamera = FALSE;
    if (!gPuppyCam.enabled || c->cutscene != 0)
    {
    if (gCurrLevelNum != LEVEL_CASTLE) {
        // If fixed camera is selected as the alternate mode, then fix the camera as long as the right
        // trigger is held
        if ((c->cutscene == 0 &&
            (gPlayer1Controller->buttonDown & R_TRIG) && cam_select_alt_mode(0) == CAM_SELECTION_FIXED)
            || (gCameraMovementFlags & CAM_MOVE_FIX_IN_PLACE)
            || (sMarioCamState->action) == ACT_GETTING_BLOWN) {

            // If this is the first frame that R_TRIG is held, play the "click" sound
            if (c->cutscene == 0 && (gPlayer1Controller->buttonPressed & R_TRIG)
                && cam_select_alt_mode(0) == CAM_SELECTION_FIXED) {
                sCameraSoundFlags |= CAM_SOUND_FIXED_ACTIVE;
                play_sound_rbutton_changed();
            }

            // Fixed mode only prevents Lakitu from moving. The camera pos still updates, so
            // Lakitu will fly to his next position as normal whenever R_TRIG is released.
            gLakituState.posHSpeed = 0.f;
            gLakituState.posVSpeed = 0.f;

            c->nextYaw = calculate_yaw(gLakituState.focus, gLakituState.pos);
            c->yaw = c->nextYaw;
            gCameraMovementFlags &= ~CAM_MOVE_FIX_IN_PLACE;
        } else {
            // Play the "click" sound when fixed mode is released
            if (sCameraSoundFlags & CAM_SOUND_FIXED_ACTIVE) {
                play_sound_rbutton_changed();
                sCameraSoundFlags &= ~CAM_SOUND_FIXED_ACTIVE;
            }
        }
    } else {
        if ((gPlayer1Controller->buttonPressed & R_TRIG) && cam_select_alt_mode(0) == CAM_SELECTION_FIXED) {
            play_sound_button_change_blocked();
        }
    }

    update_lakitu(c);
    }
    //Just a cute little bit that syncs puppycamera up to vanilla when playing a vanilla cutscene :3
    if (c->cutscene != 0)
    {
        gPuppyCam.yawTarget = gCamera->yaw;
        gPuppyCam.yaw = gCamera->yaw;
        if (gMarioState->action == ACT_ENTERING_STAR_DOOR)
        { //god this is stupid and the fact I have to continue doing this is testament to the idiocy of the star door cutscene >:(
            gPuppyCam.yawTarget = gMarioState->faceAngle[1]+0x8000;
            gPuppyCam.yaw = gMarioState->faceAngle[1]+0x8000;
        }
    }

    if (c->cutscene == 0 && gPuppyCam.enabled)
    {
        // Clear the recent cutscene after 8 frames
        if (gRecentCutscene != 0 && sFramesSinceCutsceneEnded < 8) {
            sFramesSinceCutsceneEnded++;
            if (sFramesSinceCutsceneEnded >= 8) {
                gRecentCutscene = 0;
                sFramesSinceCutsceneEnded = 0;
            }
        }
        puppycam_loop();
    }

    gLakituState.lastFrameAction = sMarioCamState->action;
}

