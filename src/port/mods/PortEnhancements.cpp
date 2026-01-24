#include "PortEnhancements.h"

#define INIT_EVENT_IDS

#include "sm64.h"
#include "camera.h"
#include "game/level_update.h"
#include "menu/title_screen.h"
#include "port/hooks/Events.h"
#include "assets/bin/segment2.h"
#include "port/ShipInit.hpp"
#include "camera/FreeLookCamera.h"

typedef enum CustomCameraMode {
    /* 0 */ CUSTOM_CAMERA_MODE_FREE_LOOK,
} CustomCameraMode;

#define CUSTOM_CAMERA_MODE(x) (CAMERA_MODE_MAX + CUSTOM_CAMERA_MODE_##x)
#define IS_CUSTOM_CAMERA(m) ((m) >= CAMERA_MODE_MAX)

static const Mtx matrix_patch_identity = {
    { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } }
};

// 0x020144B0 - 0x020144F0
static const Mtx matrix_patch_fullscreen = { { { 2.0f / SCREEN_WIDTH, 0.0f, 0.0f, 0.0f },
                                               { 0.0f, 2.0f / SCREEN_HEIGHT, 0.0f, 0.0f },
                                               { 0.0f, 0.0f, -1.0f, 0.0f },
                                               { -1.0f, -1.0f, -1.0f, 1.0f } } };

void PatchSetupDList() {
    Gfx identity = gsSPMatrix(&matrix_patch_identity, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    Gfx fullscreen = gsSPMatrix(&matrix_patch_fullscreen, G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH);
    Gfx model = gsSPMatrix(&matrix_patch_identity, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    Gfx nop = gsSPNoOp();

    // 0
    GfxPatch pt_mtx_fullscreen[] = { { 4, identity }, { 5, nop },   { 6, fullscreen },
                                     { 7, nop },      { 8, model }, { 9, nop } };
    ResourceMgr_PatchGfxByName(dl_proj_mtx_fullscreen, "SetupFullscreenProjMtx", pt_mtx_fullscreen,
                               ARRAY_COUNT(pt_mtx_fullscreen));

    // 1
    GfxPatch pt_skybox_begin[] = { { 6, identity }, { 7, nop } };
    ResourceMgr_PatchGfxByName(dl_skybox_begin, "SetupSkyboxBegin", pt_skybox_begin, ARRAY_COUNT(pt_skybox_begin));

    // 2
    GfxPatch pt_skybox_tile_settings[] = { { 0, model }, { 1, nop } };
    ResourceMgr_PatchGfxByName(dl_skybox_tile_tex_settings, "SetupSkyboxTileTexSettings", pt_skybox_tile_settings,
                               ARRAY_COUNT(pt_skybox_tile_settings));

    // 3
    GfxPatch pt_up_arrow[] = { { 7, identity }, { 8, nop } };
    ResourceMgr_PatchGfxByName(dl_ia8_up_arrow_begin, "SetupUpArrowBegin", pt_up_arrow, ARRAY_COUNT(pt_up_arrow));
}

void PortEnhancements_Init() {
    PortEnhancements_Register();
    PatchSetupDList();

    // Register event listeners
    REGISTER_LISTENER(PlayerHealthChange, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        PlayerHealthChange* ev = (PlayerHealthChange*)event;
        if (CVarGetInteger("gCheats.InfiniteHealth", 0) == 0 || ev->health > 0) {
            return;
        }

        event->cancelled = true;
    });
    REGISTER_LISTENER(PlayerLivesChange, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        PlayerLivesChange* ev = (PlayerLivesChange*)event;
        if (CVarGetInteger("gCheats.InfiniteLives", 0) == 0 || ev->lives > 0) {
            return;
        }

        event->cancelled = true;
    });
    REGISTER_LISTENER(RenderPauseCourseOptions, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        if (CVarGetInteger("gCheats.PauseExitWhenever", 0) == 0) {
            return;
        }

        RenderPauseCourseOptions* ev = (RenderPauseCourseOptions*)event;
        *ev->render = true;
    });

    REGISTER_LISTENER(LevelInitFromSaveFile, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        if (CVarGetInteger("gEnhancements.DisableLakituCutscene", 0)) {
            gNeverEnteredCastle = false;
        }
    });

    REGISTER_LISTENER(SetCameraMode, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        if (CVarGetInteger("gEnhancements.FreeLookCamera", 0)) {
            return;
        }

        event->cancelled = true;
    });

    REGISTER_LISTENER(CameraUpdate, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        CameraUpdate* ev = (CameraUpdate*)event;
        struct Camera* c = ev->c;
        static bool wasActive = false;

        if (CVarGetInteger("gEnhancements.FreeLookCamera", 0)) {
            if (!wasActive) {
                gCameraMovementFlags |= CAM_MOVE_INIT_CAMERA;
                wasActive = true;
            }

            switch (c->mode) {
                case CAMERA_MODE_CLOSE:
                case CAMERA_MODE_RADIAL:
                case CAMERA_MODE_8_DIRECTIONS:
                case CAMERA_MODE_CLOSE:
                    c->mode = CUSTOM_CAMERA_MODE(FREE_LOOK);
                    break;
            }

            if (gMarioState->action == ACT_SHOT_FROM_CANNON) {
                gMarioState->area->camera->mode = CUSTOM_CAMERA_MODE(FREE_LOOK);
                gLakituState.mode = CUSTOM_CAMERA_MODE(FREE_LOOK);
            }

            if (!IS_CUSTOM_CAMERA(c->mode)) {
                event->cancelled = true;
                return;
            }

            switch (c->mode) {
                case CUSTOM_CAMERA_MODE(FREE_LOOK):
                    FreeLookCameraUpdate(c);
                    break;
                default:
                    break;
            }


            return;
        }
        
        if (IS_CUSTOM_CAMERA(c->mode)) {
            c->mode = CAMERA_MODE_NONE;
        }
        
        wasActive = false;
        event->cancelled = true;
    });

    REGISTER_LISTENER(CameraInit, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        CameraUpdate* ev = (CameraUpdate*)event;
        struct Camera* c = ev->c;
        if (CVarGetInteger("gEnhancements.FreeLookCamera", 0)) {
            FreeLookCameraInit(c);
            return;
        }
    });
}

void PortEnhancements_Register() {
    // Register engine events
    REGISTER_EVENT(GameFrameUpdate);
    REGISTER_EVENT(LevelInitFromSaveFile);
    REGISTER_EVENT(GeoLayoutCallASM);
    REGISTER_EVENT(LevelScriptCallLoop);
    REGISTER_EVENT(LevelScriptBeginArea);
    REGISTER_EVENT(RenderPauseCourseOptions);
    REGISTER_EVENT(SetCameraMode);
    REGISTER_EVENT(CameraInit);
    REGISTER_EVENT(CameraUpdate);

    REGISTER_EVENT(PlayerHealthChange);
    REGISTER_EVENT(PlayerLivesChange);
}