/*
 * HID (Human Interface Device) Input Layer for Lockpick RCM Pro
 * Unified input handling for Joy-Con, touch, and physical buttons
 *
 * Based on TegraExplorer HID implementation
 */

#include "hid.h"
#include <input/joycon.h>
#include <utils/btn.h>
#include "../gfx/gfx.h"
#include <utils/types.h>
#include <utils/util.h>
#include <display/di.h>
#include "../config.h"
#include <libs/fatfs/ff.h>

// Forward declaration for screenshot function
extern int save_fb_to_bmp();

static Input_t inputs = {0};
u16 LbaseX = 0, LbaseY = 0, RbaseX = 0, RbaseY = 0;

// Track previous Capture button state to detect press edge (not hold)
static bool cap_was_pressed = false;

void hidInit(){
    jc_init_hw();

    // Initialize Capture button state to prevent false trigger on startup
    jc_gamepad_rpt_t *controller = joycon_poll();
    if (controller != NULL)
        cap_was_pressed = controller->cap;
}

extern hekate_config h_cfg;

Input_t *hidRead(){
    jc_gamepad_rpt_t *controller = joycon_poll();

    inputs.buttons = 0;
    u8 left_connected = 0;
    u8 right_connected = 0;

    if (controller != NULL){
        // Handle Capture button for screenshot - only on press (not hold)
        // Use edge detection to prevent multiple triggers
        if (controller->cap && !cap_was_pressed)
        {
            // Ensure SD directory exists
            f_mkdir("sd:/switch");
            f_mkdir("sd:/switch/screenshot");

            save_fb_to_bmp();

            // Visual feedback - flash brightness (like TegraExplorer)
            display_backlight_brightness(255, 1000);
            msleep(100);
            display_backlight_brightness(h_cfg.backlight, 1000);
        }

        // Update state for next time
        cap_was_pressed = controller->cap;

        inputs.buttons = controller->buttons;

        left_connected = controller->conn_l;
        right_connected = controller->conn_r;
    }


    u8 btn = btn_read();
    inputs.volp = (btn & BTN_VOL_UP) ? 1 : 0;
    inputs.volm = (btn & BTN_VOL_DOWN) ? 1 : 0;
    inputs.power = (btn & BTN_POWER) ? 1 : 0;

    if (left_connected){
        if ((LbaseX == 0 || LbaseY == 0) || controller->l3){
            LbaseX = controller->lstick_x;
            LbaseY = controller->lstick_y;
        }

        inputs.up = (controller->up || inputs.volp || (controller->lstick_y > LbaseY + 500)) ? 1 : 0;
        inputs.down = (controller->down || inputs.volm || (controller->lstick_y < LbaseY - 500)) ? 1 : 0;
        inputs.left = (controller->left || (controller->lstick_x < LbaseX - 500)) ? 1 : 0;
        inputs.right = (controller->right || (controller->lstick_x > LbaseX + 500)) ? 1 : 0;
    }
    else {
        inputs.up = inputs.volp;
        inputs.down = inputs.volm;
    }

    if (right_connected){
        if ((RbaseX == 0 || RbaseY == 0) || controller->r3){
            RbaseX = controller->rstick_x;
            RbaseY = controller->rstick_y;
        }

        inputs.rUp = (controller->rstick_y > RbaseY + 500) ? 1 : 0;
        inputs.rDown = (controller->rstick_y < RbaseY - 500) ? 1 : 0;
        inputs.rLeft = (controller->rstick_x < RbaseX - 500) ? 1 : 0;
        inputs.rRight = (controller->rstick_x > RbaseX + 500) ? 1 : 0;
    }
    inputs.a = inputs.a || inputs.power;

    return &inputs;
}

Input_t *hidWaitMask(u32 mask){
    Input_t *in = hidRead();

    while (in->buttons & mask)
        hidRead();

    while (!(in->buttons & mask)){
        hidRead();
    }

    return in;
}

Input_t *hidWait(){
    Input_t *in = hidRead();

    while (in->buttons)
        hidRead();

    while (!(in->buttons))
        hidRead();
    return in;
}

bool hidConnected(){
    jc_gamepad_rpt_t *controller = joycon_poll();
    return (controller->conn_l && controller->conn_r) ? 1 : 0;
}
