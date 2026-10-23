#include <kf/lib/address.h>
#include <kf/open/opening.h>
#include <kf/lib/audio.h>
#include <kf/lib/display.h>
#include <psyq/pad.h>

/* Title elements fade in to PEAK_LEVEL, then settle back to REST_LEVEL. */
enum {
    TITLE_FADE_STEP = 6,
    TITLE_PEAK_LEVEL = 144,
    TITLE_REST_LEVEL = 96,
    PROMPT_PULSE_STEP = 3,
    PROMPT_PULSE_HIGH = 97,
    PROMPT_PULSE_LOW = 48,
    TITLE_BACKDROP_LEVEL = 32,
    TITLE_BACKDROP_WIDTH = 640,
    TITLE_BACKDROP_OT_DEPTH = 100,
    TITLE_OT_DEPTH = 200,
    /* The banner is two abutting quads on one line of the title screen. */
    BANNER_LEFT_X = 132,
    BANNER_RIGHT_X = 388,
    BANNER_Y = 172,
    BANNER_LEFT_WIDTH = 255,
    BANNER_RIGHT_WIDTH = 128,
    BANNER_HEIGHT = 32,
    PRIMITIVE_BRIGHTNESS = 64,
    /* Voice played when a title choice is made. */
    CHOICE_VOICE_PROGRAM = 10,
    CHOICE_VOICE_NOTE = 60,
    CHOICE_VOICE_VOLUME = 64
};

DATA(0x8003e724, 0x4, ".sdata")
s32 title_level = 0;
DATA(0x8003e728, 0x4, ".sdata")
KfTitlePhase title_phase = KF_TITLE_PHASE_FADE_IN;
DATA(0x8003e72c, 0x4, ".sdata")
s32 banner_level = 0;
DATA(0x8003e730, 0x4, ".sdata")
KfTitlePhase banner_phase = KF_TITLE_PHASE_FADE_IN;
DATA(0x8003e734, 0x4, ".sdata")
s32 prompt_level = PROMPT_PULSE_LOW;
DATA(0x8003e738, 0x4, ".sdata")
KfPromptPhase prompt_phase = KF_PROMPT_PHASE_BRIGHTEN;
DATA(0x8003e73c, 0x4, ".sdata")
u32 pad_previous_buttons = 0;

/* Draws the backdrop and the four title tiles; returns true once settled. */
ADDRESS(0x80012d28, 0x61c)
b32 opening_draw_title(KfTitleMode mode)
{
    b32 settled = KF_FALSE;

    if (mode == KF_TITLE_SHOW) {
        title_phase = KF_TITLE_PHASE_SETTLE;
        title_level = TITLE_REST_LEVEL;
    } else if (mode == KF_TITLE_RESET) {
        title_level = 0;
        title_phase = KF_TITLE_PHASE_FADE_IN;
        return KF_FALSE;
    }
    primitive_buffer_begin_poly_ft4();
    setRGB0(current_poly_ft4, TITLE_BACKDROP_LEVEL, TITLE_BACKDROP_LEVEL, TITLE_BACKDROP_LEVEL);
    setTPage(current_poly_ft4, 2, 0, 640, 0);
    setXYWH(current_poly_ft4, 0, 0, TITLE_BACKDROP_WIDTH, KF_DISPLAY_HEIGHT);
    setUVWH(current_poly_ft4, 0, 0, 242, 239);
    primitive_buffer_commit_poly_ft4(TITLE_BACKDROP_OT_DEPTH);
    if (title_phase == KF_TITLE_PHASE_FADE_IN) {
        title_level += TITLE_FADE_STEP;
        if (title_level >= TITLE_PEAK_LEVEL) {
            title_phase = KF_TITLE_PHASE_SETTLE;
        }
    } else if (title_level > TITLE_REST_LEVEL) {
        title_level -= TITLE_FADE_STEP;
    } else {
        settled = KF_TRUE;
    }
    primitive_buffer_begin_poly_ft4();
    setTPage(current_poly_ft4, 2, 1, 640, 256);
    setRGB0(current_poly_ft4, title_level, title_level, title_level);
    setXYWH(current_poly_ft4, 20, 55, 256, 121);
    setUVWH(current_poly_ft4, 0, 0, 255, 121);
    primitive_buffer_commit_poly_ft4(TITLE_OT_DEPTH);
    primitive_buffer_begin_poly_ft4();
    setTPage(current_poly_ft4, 2, 1, 896, 256);
    setRGB0(current_poly_ft4, title_level, title_level, title_level);
    setXYWH(current_poly_ft4, 276, 55, 44, 121);
    setUVWH(current_poly_ft4, 0, 0, 44, 121);
    primitive_buffer_commit_poly_ft4(TITLE_OT_DEPTH);
    primitive_buffer_begin_poly_ft4();
    setTPage(current_poly_ft4, 2, 1, 640, 256);
    setRGB0(current_poly_ft4, title_level, title_level, title_level);
    setXYWH(current_poly_ft4, 320, 55, 256, 121);
    setUVWH(current_poly_ft4, 0, 128, 255, 121);
    primitive_buffer_commit_poly_ft4(TITLE_OT_DEPTH);
    primitive_buffer_begin_poly_ft4();
    setTPage(current_poly_ft4, 2, 1, 896, 256);
    setRGB0(current_poly_ft4, title_level, title_level, title_level);
    setXYWH(current_poly_ft4, 576, 55, 44, 121);
    setUVWH(current_poly_ft4, 0, 128, 44, 121);
    primitive_buffer_commit_poly_ft4(TITLE_OT_DEPTH);
    return settled;
}

/* Draws the banner under the title; returns true once settled. */
ADDRESS(0x80013344, 0x33c)
b32 opening_draw_banner(KfTitleMode mode)
{
    b32 settled = KF_FALSE;

    if (mode == KF_TITLE_SHOW) {
        banner_phase = KF_TITLE_PHASE_SETTLE;
        banner_level = TITLE_REST_LEVEL;
    } else if (mode == KF_TITLE_RESET) {
        banner_level = 0;
        banner_phase = KF_TITLE_PHASE_FADE_IN;
        return KF_FALSE;
    }
    if (banner_phase == KF_TITLE_PHASE_FADE_IN) {
        banner_level += TITLE_FADE_STEP;
        if (banner_level >= TITLE_PEAK_LEVEL) {
            banner_phase = KF_TITLE_PHASE_SETTLE;
        }
    } else if (banner_level > TITLE_REST_LEVEL) {
        banner_level -= TITLE_FADE_STEP;
    } else {
        settled = KF_TRUE;
    }
    primitive_buffer_begin_poly_ft4();
    setTPage(current_poly_ft4, 0, 1, 896, 0);
    current_poly_ft4->clut = getClut(0, 500);
    SetSemiTrans((void *)current_poly_ft4, 1);
    setRGB0(current_poly_ft4, banner_level, banner_level, banner_level);
    setXYWH(current_poly_ft4, BANNER_LEFT_X, BANNER_Y, BANNER_LEFT_WIDTH, BANNER_HEIGHT);
    setUVWH(current_poly_ft4, 0, 0, BANNER_LEFT_WIDTH, BANNER_HEIGHT);
    primitive_buffer_commit_poly_ft4(TITLE_OT_DEPTH);
    primitive_buffer_begin_poly_ft4();
    setTPage(current_poly_ft4, 0, 1, 896, 0);
    current_poly_ft4->clut = getClut(0, 500);
    SetSemiTrans((void *)current_poly_ft4, 1);
    setRGB0(current_poly_ft4, banner_level, banner_level, banner_level);
    setXYWH(current_poly_ft4, BANNER_RIGHT_X, BANNER_Y, BANNER_RIGHT_WIDTH, BANNER_HEIGHT);
    setUVWH(current_poly_ft4, 0, BANNER_HEIGHT, BANNER_RIGHT_WIDTH, BANNER_HEIGHT);
    primitive_buffer_commit_poly_ft4(TITLE_OT_DEPTH);
    return settled;
}

/* Draws the prompt; KF_TITLE_ANIMATE makes it pulse. */
ADDRESS(0x80013680, 0x1c0)
void opening_draw_prompt(KfTitleMode mode)
{
    if (prompt_phase == KF_PROMPT_PHASE_BRIGHTEN) {
        prompt_level += PROMPT_PULSE_STEP;
        if (prompt_level >= PROMPT_PULSE_HIGH) {
            prompt_phase = KF_PROMPT_PHASE_DIM;
        }
    } else {
        prompt_level -= PROMPT_PULSE_STEP;
        if (prompt_level < PROMPT_PULSE_LOW) {
            prompt_phase = KF_PROMPT_PHASE_BRIGHTEN;
        }
    }
    primitive_buffer_begin_poly_ft4();
    setTPage(current_poly_ft4, 0, 1, 896, 0);
    current_poly_ft4->clut = getClut(0, 501);
    SetSemiTrans((void *)current_poly_ft4, 1);
    if (mode == KF_TITLE_ANIMATE) {
        setRGB0(current_poly_ft4, prompt_level, prompt_level, prompt_level);
    }
    setXYWH(current_poly_ft4, 287, 204, 64, 12);
    setUVWH(current_poly_ft4, 0, 128, 64, 12);
    primitive_buffer_commit_poly_ft4(TITLE_OT_DEPTH);
}

/* A newly pressed D-pad direction, START or SELECT starts the game. Counts
 * every polled frame in IDLE_FRAMES. */
ADDRESS(0x80013840, 0x144)
KfOpeningChoice opening_poll_pad(const KfTitleMode *prompt_mode, s32 *idle_frames)
{
    KfOpeningChoice choice = KF_OPENING_NO_CHOICE;
    u32 buttons = PadRead(1);

    if ((buttons & PADRup && !(pad_previous_buttons & PADRup))
        || (buttons & PADRright && !(pad_previous_buttons & PADRright))
        || (buttons & PADRdown && !(pad_previous_buttons & PADRdown))
        || (buttons & PADRleft && !(pad_previous_buttons & PADRleft))
        || (buttons & PADstart && !(pad_previous_buttons & PADstart))
        || (buttons & PADselect && !(pad_previous_buttons & PADselect))) {
        audio_play_voice(audio_vab_id, CHOICE_VOICE_PROGRAM, 0, CHOICE_VOICE_NOTE,
            CHOICE_VOICE_VOLUME, CHOICE_VOICE_VOLUME);
        choice = KF_OPENING_START_GAME;
    }
    pad_previous_buttons = buttons;
    (*idle_frames)++;
    return choice;
}

ADDRESS(0x80013984, 0x50)
void primitive_buffer_begin_poly_ft4(void)
{
    SetPolyFT4(current_poly_ft4);
    setRGB0(current_poly_ft4, PRIMITIVE_BRIGHTNESS, PRIMITIVE_BRIGHTNESS, PRIMITIVE_BRIGHTNESS);
}

/* Darkens the quad by display_fade_level, then links it at DEPTH. */
ADDRESS(0x800139d4, 0xcc)
void primitive_buffer_commit_poly_ft4(s32 depth)
{
    u8 level;

    if (display_fade_level != 0) {
        level = current_poly_ft4->r0;
        if (level < display_fade_level) {
            level = 0;
        } else {
            level -= display_fade_level;
        }
        setRGB0(current_poly_ft4, level, level, level);
    }
    AddPrim((void *)&display_current->ordering_table[depth], (void *)current_poly_ft4);
    current_poly_ft4++;
}
