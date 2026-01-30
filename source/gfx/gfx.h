/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018-2021 CTCaer
 * Copyright (c) 2019-2021 shchmue
 * Copyright (c) 2018 M4xw
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _GFX_H_
#define _GFX_H_

#include <utils/types.h>

#define EPRINTF(text) gfx_printf("%k"text"%k\n", 0xFFFF0000, 0xFFCCCCCC)
#define EPRINTFARGS(text, args...) gfx_printf("%k"text"%k\n", 0xFFFF0000, args, 0xFFCCCCCC)
#define WPRINTF(text) gfx_printf("%k"text"%k\n", 0xFFFFDD00, 0xFFCCCCCC)
#define WPRINTFARGS(text, args...) gfx_printf("%k"text"%k\n", 0xFFFFDD00, args, 0xFFCCCCCC)

// ============================================================================
// Global UI Layout Settings
// ============================================================================
// Change these values to adjust menu/text positions across ALL screens
// ============================================================================

// Main Menu Position (tui.c)
// NOTE: 16px is automatically added by leading space in menu text
#define UI_MENU_START_X    5    // Base position (text actually starts at +16px)
#define UI_MENU_START_Y    32   // Vertical position of menu items (from top, below title bar)
#define UI_MENU_SPACING    24   // Vertical spacing between menu items

// Dump/Content Screens Position (keys.c, dump functions)
// +16 aligns with menu text position (accounting for menu's leading space)
#define UI_CONTENT_START_X (UI_MENU_START_X + 16)  // Aligned with menu text
#define UI_CONTENT_START_Y 32   // Vertical position for dump text
#define UI_CONTENT_PARTIAL_Y 96 // Vertical position for partial keys dump

// Screenshot Notification Position (tui.c)
#define UI_NOTIFY_X        5    // Horizontal position for notifications
#define UI_NOTIFY_Y        680  // Vertical position for notifications (above bottom bar)

// ============================================================================
// Landscape Mode Text Positioning Template (1280x720 screen, 16x16 rotated font)
// ============================================================================
// Font: 16 pixels per character (in landscape, spaces are 16px wide)
// Screen: 1280px wide × 720px tall

// Margins in pixels (calculated from UI_MENU_START_X/Y)
#define GFX_LANDSCAPE_MARGIN_LEFT_PX      (UI_MENU_START_X + 16)    // Text start position
#define GFX_LANDSCAPE_MARGIN_TOP_PX       (1279 - UI_MENU_START_X)  // Internal Y position

// Margin in character spaces (0 spaces - positioning done via gfx_con_setpos)
#define GFX_LANDSCAPE_MARGIN_SPACES        0     // 0 spaces - pos set via gfx_con_setpos

// Full text line template (30 chars + "done in" + timing)
// Used for aligned progress display
#define GFX_LANDSCAPE_LINE_WIDTH_CHARS   30    // 21 chars for label/padding + 9 chars for "done in" + timing

// Pre-built space strings for common padding (for "done in" alignment)
// Label + padding = 21 chars, then "done in"
#define GFX_PAD_AFTER_LABEL_21           "             " // 13 spaces (for 8-char labels)
#define GFX_PAD_AFTER_LABEL_13           "          "   // 10 spaces (for 11-char labels)
#define GFX_PAD_AFTER_LABEL_10           "             " // 13 spaces (for 8-char labels)
#define GFX_PAD_AFTER_LABEL_9            "              "  // 14 spaces (for 7-char labels)
#define GFX_PAD_FOR_DONE_IN             7     // " done in" = 7 chars

// Template string for left margin (use at start of printf lines)
#define GFX_LANDSCAPE_MARGIN_STR          ""     // 0 spaces (set pos via gfx_con_setpos)

// Calculate padding: spaces = GFX_LANDSCAPE_LINE_WIDTH_CHARS - (label_len + 7)
// Example: "MMC init..." (10) → spaces = 30 - (10 + 7) = 13 spaces
// Note: Add color codes BEFORE label if needed: "%kLabel%k%kPadding%s done in..."
//
// Positioning in landscape mode (1280x720, rotated 90°):
// - gfx_con_setpos(30, 80) → Internal: x=80, y=1249
// - After newline: x=96, y=1249 (x increments by 16 for next column)
// - GFX_LANDSCAPE_MARGIN_STR (1 space) → Text starts at 96+16=112px
// - Menu text starts at 80+16=96px (the " " space in menu printf)

typedef struct _gfx_ctxt_t
{
	u32 *fb;
	u32 width;
	u32 height;
	u32 stride;
} gfx_ctxt_t;

typedef struct _gfx_con_t
{
	gfx_ctxt_t *gfx_ctxt;
	u32 fntsz;
	u32 x;
	u32 y;
	u32 savedx;
	u32 savedy;
	u32 fgcol;
	int fillbg;
	u32 bgcol;
	bool mute;
} gfx_con_t;

// Global gfx console and context.
extern gfx_ctxt_t gfx_ctxt;
extern gfx_con_t gfx_con;

// Global YLeftConfig and XTopConfig for margin tracking (like TegraExplorer)
extern u32 g_YLeftConfig;  // Tracks left margin (y in landscape mode)
extern u32 g_XTopConfig;   // Tracks top margin (x in landscape mode)

void gfx_init_ctxt(u32 *fb, u32 width, u32 height, u32 stride);
void gfx_clear_grey(u8 color);
void gfx_clear_partial_grey(u8 color, u32 pos_x, u32 height);
void gfx_clear_color(u32 color);
void gfx_con_init();
void gfx_con_setcol(u32 fgcol, int fillbg, u32 bgcol);
void gfx_con_getpos(u32 *x, u32 *y);
void gfx_con_setpos(u32 x, u32 y);
void gfx_putc(char c);
void gfx_puts(const char *s);
void gfx_printf(const char *fmt, ...);
void gfx_hexdump(u32 base, const void *buf, u32 len);
void gfx_hexdiff(u32 base, const void *buf1, const void *buf2, u32 len);

void gfx_set_pixel(u32 x, u32 y, u32 color);
void gfx_line(int x0, int y0, int x1, int y1, u32 color);
void gfx_put_small_sep();
void gfx_put_big_sep();
void gfx_printf_centered(u32 y, const char *fmt, ...);
void gfx_set_rect_grey(const u8 *buf, u32 size_x, u32 size_y, u32 pos_x, u32 pos_y);
void gfx_set_rect_rgb(const u8 *buf, u32 size_x, u32 size_y, u32 pos_x, u32 pos_y);
void gfx_set_rect_argb(const u32 *buf, u32 size_x, u32 size_y, u32 pos_x, u32 pos_y);
void gfx_render_bmp_argb(const u32 *buf, u32 size_x, u32 size_y, u32 pos_x, u32 pos_y);

void gfx_draw_title_bar(const char *title);
void gfx_draw_bottom_bar(const char *legend);

#endif
