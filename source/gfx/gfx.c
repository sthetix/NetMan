/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018-2021 CTCaer
 * Copyright (c) 2019-2021 shchmue
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

#include <stdarg.h>
#include <string.h>
#include "gfx.h"

// Global gfx console and context.
gfx_ctxt_t gfx_ctxt;
gfx_con_t gfx_con;
u32 g_YLeftConfig = 1279; // Default to left edge (YLEFT)
u32 g_XTopConfig = 0;     // Default to top edge (XTOP) - tracks initial column for newlines

static bool gfx_con_init_done = false;

static const u8 _gfx_font[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Char 032 ( )
	0x00, 0x30, 0x30, 0x18, 0x18, 0x00, 0x0C, 0x00, // Char 033 (!)
	0x00, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, // Char 034 (")
	0x00, 0x66, 0x66, 0xFF, 0x66, 0xFF, 0x66, 0x66, // Char 035 (#)
	0x00, 0x18, 0x7C, 0x06, 0x3C, 0x60, 0x3E, 0x18, // Char 036 ($)
	0x00, 0x46, 0x66, 0x30, 0x18, 0x0C, 0x66, 0x62, // Char 037 (%)
	0x00, 0x3C, 0x66, 0x3C, 0x1C, 0xE6, 0x66, 0xFC, // Char 038 (&)
	0x00, 0x18, 0x0C, 0x06, 0x00, 0x00, 0x00, 0x00, // Char 039 (')
	0x00, 0x30, 0x18, 0x0C, 0x0C, 0x18, 0x30, 0x00, // Char 040 (()
	0x00, 0x0C, 0x18, 0x30, 0x30, 0x18, 0x0C, 0x00, // Char 041 ())
	0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00, // Char 042 (*)
	0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00, // Char 043 (+)
	0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x0C, 0x00, // Char 044 (,)
	0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, // Char 045 (-)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, // Char 046 (.)
	0x00, 0x40, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x00, // Char 047 (/)
	0x00, 0x3C, 0x66, 0x76, 0x6E, 0x66, 0x3C, 0x00, // Char 048 (0)
	0x00, 0x18, 0x1C, 0x18, 0x18, 0x18, 0x7E, 0x00, // Char 049 (1)
	0x00, 0x3C, 0x62, 0x30, 0x0C, 0x06, 0x7E, 0x00, // Char 050 (2)
	0x00, 0x3C, 0x62, 0x38, 0x60, 0x66, 0x3C, 0x00, // Char 051 (3)
	0x00, 0x6C, 0x6C, 0x66, 0xFE, 0x60, 0x60, 0x00, // Char 052 (4)
	0x00, 0x7E, 0x06, 0x7E, 0x60, 0x66, 0x3C, 0x00, // Char 053 (5)
	0x00, 0x3C, 0x06, 0x3E, 0x66, 0x66, 0x3C, 0x00, // Char 054 (6)
	0x00, 0x7E, 0x30, 0x30, 0x18, 0x18, 0x18, 0x00, // Char 055 (7)
	0x00, 0x3C, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00, // Char 056 (8)
	0x00, 0x3C, 0x66, 0x7C, 0x60, 0x66, 0x3C, 0x00, // Char 057 (9)
	0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x18, 0x00, // Char 058 (:)
	0x00, 0x00, 0x18, 0x00, 0x18, 0x18, 0x0C, 0x00, // Char 059 (;)
	0x00, 0x70, 0x1C, 0x06, 0x06, 0x1C, 0x70, 0x00, // Char 060 (<)
	0x00, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x00, 0x00, // Char 061 (=)
	0x00, 0x0E, 0x38, 0x60, 0x60, 0x38, 0x0E, 0x00, // Char 062 (>)
	0x00, 0x3C, 0x66, 0x30, 0x18, 0x00, 0x18, 0x00, // Char 063 (?)
	0x00, 0x3C, 0x66, 0x76, 0x76, 0x06, 0x46, 0x3C, // Char 064 (@)
	0x00, 0x3C, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00, // Char 065 (A)
	0x00, 0x3E, 0x66, 0x3E, 0x66, 0x66, 0x3E, 0x00, // Char 066 (B)
	0x00, 0x3C, 0x66, 0x06, 0x06, 0x66, 0x3C, 0x00, // Char 067 (C)
	0x00, 0x1E, 0x36, 0x66, 0x66, 0x36, 0x1E, 0x00, // Char 068 (D)
	0x00, 0x7E, 0x06, 0x1E, 0x06, 0x06, 0x7E, 0x00, // Char 069 (E)
	0x00, 0x3E, 0x06, 0x1E, 0x06, 0x06, 0x06, 0x00, // Char 070 (F)
	0x00, 0x3C, 0x66, 0x06, 0x76, 0x66, 0x3C, 0x00, // Char 071 (G)
	0x00, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00, // Char 072 (H)
	0x00, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, // Char 073 (I)
	0x00, 0x78, 0x30, 0x30, 0x30, 0x36, 0x1C, 0x00, // Char 074 (J)
	0x00, 0x66, 0x36, 0x1E, 0x1E, 0x36, 0x66, 0x00, // Char 075 (K)
	0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0x7E, 0x00, // Char 076 (L)
	0x00, 0x46, 0x6E, 0x7E, 0x56, 0x46, 0x46, 0x00, // Char 077 (M)
	0x00, 0x66, 0x6E, 0x7E, 0x76, 0x66, 0x66, 0x00, // Char 078 (N)
	0x00, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00, // Char 079 (O)
	0x00, 0x3E, 0x66, 0x3E, 0x06, 0x06, 0x06, 0x00, // Char 080 (P)
	0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x70, 0x00, // Char 081 (Q)
	0x00, 0x3E, 0x66, 0x3E, 0x1E, 0x36, 0x66, 0x00, // Char 082 (R)
	0x00, 0x3C, 0x66, 0x0C, 0x30, 0x66, 0x3C, 0x00, // Char 083 (S)
	0x00, 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, // Char 084 (T)
	0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00, // Char 085 (U)
	0x00, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00, // Char 086 (V)
	0x00, 0x46, 0x46, 0x56, 0x7E, 0x6E, 0x46, 0x00, // Char 087 (W)
	0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00, // Char 088 (X)
	0x00, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00, // Char 089 (Y)
	0x00, 0x7E, 0x30, 0x18, 0x0C, 0x06, 0x7E, 0x00, // Char 090 (Z)
	0x00, 0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, // Char 091 ([)
	0x00, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00, // Char 092 (\)
	0x00, 0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, // Char 093 (])
	0x00, 0x18, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, // Char 094 (^)
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, // Char 095 (_)
	0x00, 0x0C, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, // Char 096 (`)
	0x00, 0x00, 0x3C, 0x60, 0x7C, 0x66, 0x7C, 0x00, // Char 097 (a)
	0x00, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3E, 0x00, // Char 098 (b)
	0x00, 0x00, 0x3C, 0x06, 0x06, 0x06, 0x3C, 0x00, // Char 099 (c)
	0x00, 0x60, 0x60, 0x7C, 0x66, 0x66, 0x7C, 0x00, // Char 100 (d)
	0x00, 0x00, 0x3C, 0x66, 0x7E, 0x06, 0x3C, 0x00, // Char 101 (e)
	0x00, 0x38, 0x0C, 0x3E, 0x0C, 0x0C, 0x0C, 0x00, // Char 102 (f)
	0x00, 0x00, 0x7C, 0x66, 0x7C, 0x40, 0x3C, 0x00, // Char 103 (g)
	0x00, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x00, // Char 104 (h)
	0x00, 0x18, 0x00, 0x1C, 0x18, 0x18, 0x3C, 0x00, // Char 105 (i)
	0x00, 0x30, 0x00, 0x30, 0x30, 0x30, 0x1E, 0x00, // Char 106 (j)
	0x00, 0x06, 0x06, 0x36, 0x1E, 0x36, 0x66, 0x00, // Char 107 (k)
	0x00, 0x1C, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, // Char 108 (l)
	0x00, 0x00, 0x66, 0xFE, 0xFE, 0xD6, 0xC6, 0x00, // Char 109 (m)
	0x00, 0x00, 0x3E, 0x66, 0x66, 0x66, 0x66, 0x00, // Char 110 (n)
	0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00, // Char 111 (o)
	0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x00, // Char 112 (p)
	0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x00, // Char 113 (q)
	0x00, 0x00, 0x3E, 0x66, 0x06, 0x06, 0x06, 0x00, // Char 114 (r)
	0x00, 0x00, 0x7C, 0x06, 0x3C, 0x60, 0x3E, 0x00, // Char 115 (s)
	0x00, 0x18, 0x7E, 0x18, 0x18, 0x18, 0x70, 0x00, // Char 116 (t)
	0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x00, // Char 117 (u)
	0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00, // Char 118 (v)
	0x00, 0x00, 0xC6, 0xD6, 0xFE, 0x7C, 0x6C, 0x00, // Char 119 (w)
	0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00, // Char 120 (x)
	0x00, 0x00, 0x66, 0x66, 0x7C, 0x60, 0x3C, 0x00, // Char 121 (y)
	0x00, 0x00, 0x7E, 0x30, 0x18, 0x0C, 0x7E, 0x00, // Char 122 (z)
	0x00, 0x18, 0x08, 0x08, 0x04, 0x08, 0x08, 0x18, // Char 123 ({)
	0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, // Char 124 (|)
	0x00, 0x0C, 0x08, 0x08, 0x10, 0x08, 0x08, 0x0C, // Char 125 (})
	0x00, 0x00, 0x00, 0x4C, 0x32, 0x00, 0x00, 0x00  // Char 126 (~)
};

void gfx_clear_grey(u8 color)
{
	// Use memset for speed - expand 8-bit grayscale to 32-bit ARGB
	// memset repeats the byte pattern, so 0x1B becomes 0x1B1B1B1B which works for grayscale
	memset(gfx_ctxt.fb, color, gfx_ctxt.width * gfx_ctxt.height * 4);
}

void gfx_clear_partial_grey(u8 color, u32 pos_x, u32 height)
{
	// Use memset for speed - pos_x = internal y position, height = number of rows
	memset(gfx_ctxt.fb + pos_x * gfx_ctxt.stride, color, height * 4 * gfx_ctxt.stride);
}

void gfx_clear_color(u32 color)
{
	for (u32 i = 0; i < gfx_ctxt.width * gfx_ctxt.height; i++)
		gfx_ctxt.fb[i] = color;
}

void gfx_init_ctxt(u32 *fb, u32 width, u32 height, u32 stride)
{
	gfx_ctxt.fb = fb;
	gfx_ctxt.width = width;
	gfx_ctxt.height = height;
	gfx_ctxt.stride = stride;
}

void gfx_con_init()
{
	gfx_con.gfx_ctxt = &gfx_ctxt;
	gfx_con.fntsz = 16;
	gfx_con.x = 0;
	gfx_con.y = 0;
	gfx_con.savedx = 0;
	gfx_con.savedy = 0;
	gfx_con.fgcol = 0xFFCCCCCC;
	gfx_con.fillbg = 1;
	gfx_con.bgcol = 0xFF1B1B1B;
	gfx_con.mute = 0;

	g_YLeftConfig = 1279; // Default to left edge
	g_XTopConfig = 0;     // Default to top edge

	gfx_con_init_done = true;
}

void gfx_con_setcol(u32 fgcol, int fillbg, u32 bgcol)
{
	gfx_con.fgcol = fgcol;
	gfx_con.fillbg = fillbg;
	gfx_con.bgcol = bgcol;
}

void gfx_con_getpos(u32 *x, u32 *y)
{
	// Software rotation for landscape mode (like TegraExplorer/warmboot-extractor)
	*x = 1279 - gfx_con.y;
	*y = gfx_con.x;
}

void gfx_con_setpos(u32 x, u32 y)
{
	// Software rotation for landscape mode (like TegraExplorer/warmboot-extractor)
	gfx_con.x = y;
	gfx_con.y = 1279 - x;

	// Safety: clamp y to valid range to prevent memory corruption
	if (gfx_con.y < 16)
		gfx_con.y = 16;
	if (gfx_con.y > 1279)
		gfx_con.y = 1279;

	g_YLeftConfig = gfx_con.y; // Track left margin for newlines (after clamp)
	g_XTopConfig = y;          // Track top margin for newlines (internal x = external y)
}

void gfx_putc(char c)
{
	// Duplicate code for performance reasons.
	switch (gfx_con.fntsz)
	{
	case 16:
		if (c >= 32 && c <= 129)
		{
			u8 *cbuf = (u8 *)&_gfx_font[8 * (c - 32)];
			u32 *fb = gfx_ctxt.fb + gfx_con.x + gfx_con.y * gfx_ctxt.stride;

			// Rotated rendering (like TegraExplorer) - renders vertically
			for (u32 i = 0; i < 16; i+=2)
			{
				u8 v = *cbuf;
				for (u32 t = 0; t < 8; t++){
					if (v & 1 || gfx_con.fillbg){
						u32 setColor = (v & 1) ? gfx_con.fgcol : gfx_con.bgcol;
						*fb = setColor;
						*(fb + 1) = setColor;
						*(fb - gfx_ctxt.stride) = setColor;
						*(fb - gfx_ctxt.stride + 1) = setColor;
					}
					v >>= 1;
					fb -= gfx_ctxt.stride * 2;
				}
				fb += gfx_ctxt.stride * 16 + 2;
				cbuf++;
			}

			gfx_con.y -= 16;
			if (gfx_con.y < 16){
				// Safety: ensure YLeftConfig and XTopConfig are valid before using them
				if (g_YLeftConfig < 16)
					g_YLeftConfig = 1279;  // Reset to top of screen in landscape mode
				if (g_XTopConfig > 1279)
					g_XTopConfig = 0;      // Reset to left edge if invalid
				gfx_con.y = g_YLeftConfig;
				gfx_con.x += 16;  // Advance to next line (downward in landscape mode)
				// Wrap to top if we go off bottom of screen
				if (gfx_con.x >= 720)
					gfx_con.x = 0;
			}
		}
		else if (c == '\n')
		{
			// Safety: ensure YLeftConfig and XTopConfig are valid
			if (g_YLeftConfig < 16)
				g_YLeftConfig = 1279;
			if (g_XTopConfig > 1279)
				g_XTopConfig = 0;
			gfx_con.y = g_YLeftConfig;
			gfx_con.x += 16;  // Advance to next line (downward in landscape mode)
			// Wrap to top if we go off bottom of screen
			if (gfx_con.x >= 720)
				gfx_con.x = 0;
		}
		else if (c == '\r')
		{
			// Carriage return: go to start of current line (not initial line)
			// Safety: ensure YLeftConfig is valid
			if (g_YLeftConfig < 16)
				g_YLeftConfig = 1279;
			gfx_con.y = g_YLeftConfig;  // Reset to left margin, keep current line (gfx_con.x unchanged)
		}
		break;
	case 8:
	default:
		if (c >= 32 && c <= 126)
		{
			u8 *cbuf = (u8 *)&_gfx_font[8 * (c - 32)];
			u32 *fb = gfx_ctxt.fb + gfx_con.x + gfx_con.y * gfx_ctxt.stride;
			for (u32 i = 0; i < 8; i++)
			{
				u8 v = *cbuf++;
				for (u32 j = 0; j < 8; j++)
				{
					if (v & 1)
						*fb = gfx_con.fgcol;
					else if (gfx_con.fillbg)
						*fb = gfx_con.bgcol;
					v >>= 1;
					fb++;
				}
				fb += gfx_ctxt.stride - 8;
			}
			gfx_con.x += 8;
			if (gfx_con.x > gfx_ctxt.width - 8)
			{
				gfx_con.x = 0;
				gfx_con.y += 8;
			}
		}
		else if (c == '\n')
		{
			gfx_con.x = 0;
			gfx_con.y += 8;
			if (gfx_con.y > gfx_ctxt.height - 8)
				gfx_con.y = 0;
		}
		break;
	}
}

void gfx_puts(const char *s)
{
	if (!s || !gfx_con_init_done || gfx_con.mute)
		return;

	// Safety: clamp y to valid range before rendering to prevent memory corruption
	if (gfx_con.y < 16)
		gfx_con.y = 16;
	if (gfx_con.y > 1279)
		gfx_con.y = 1279;

	for (; *s; s++)
		gfx_putc(*s);
}

static void _gfx_putn(u32 v, int base, char fill, int fcnt)
{
	char buf[65];
	static const char digits[] = "0123456789ABCDEFghijklmnopqrstuvwxyz";
	char *p;
	int c = fcnt;

	if (base > 36)
		return;

	p = buf + 64;
	*p = 0;
	do
	{
		c--;
		*--p = digits[v % base];
		v /= base;
	} while (v);

	if (fill != 0)
	{
		while (c > 0)
		{
			*--p = fill;
			c--;
		}
	}

	gfx_puts(p);
}

void gfx_put_small_sep()
{
	u8 prevFontSize = gfx_con.fntsz;
	gfx_con.fntsz = 8;
	gfx_putc('\n');
	gfx_con.fntsz = prevFontSize;
}

void gfx_put_big_sep()
{
	u8 prevFontSize = gfx_con.fntsz;
	gfx_con.fntsz = 16;
	gfx_putc('\n');
	gfx_con.fntsz = prevFontSize;
}

void gfx_printf(const char *fmt, ...)
{
	if (!gfx_con_init_done || gfx_con.mute)
		return;

	va_list ap;
	int fill, fcnt;

	va_start(ap, fmt);
	while(*fmt)
	{
		if(*fmt == '%')
		{
			fmt++;
			fill = 0;
			fcnt = 0;
			if ((*fmt >= '0' && *fmt <= '9') || *fmt == ' ')
			{
				fcnt = *fmt;
				fmt++;
				if (*fmt >= '0' && *fmt <= '9')
				{
					fill = fcnt;
					fcnt = *fmt - '0';
					fmt++;
				}
				else
				{
					fill = ' ';
					fcnt -= '0';
				}
			}
			switch(*fmt)
			{
			case 'c':
				gfx_putc(va_arg(ap, u32));
				break;
			case 's':
				gfx_puts(va_arg(ap, char *));
				break;
			case 'd':
				_gfx_putn(va_arg(ap, u32), 10, fill, fcnt);
				break;
			case 'p':
			case 'P':
			case 'x':
			case 'X':
				_gfx_putn(va_arg(ap, u32), 16, fill, fcnt);
				break;
			case 'k':
				gfx_con.fgcol = va_arg(ap, u32);
				break;
			case 'K':
				gfx_con.bgcol = va_arg(ap, u32);
				gfx_con.fillbg = 1;
				break;
			case '%':
				gfx_putc('%');
				break;
			case '\0':
				goto out;
			default:
				gfx_putc('%');
				gfx_putc(*fmt);
				break;
			}
		}
		else
			gfx_putc(*fmt);
		fmt++;
	}

	out:
	va_end(ap);
}

void gfx_printf_centered(u32 y, const char *fmt, ...)
{
	// Simplified version: just center by manually positioning
	// Use gfx_printf directly without vsnprintf to avoid stdio dependency
	va_list ap;
	va_start(ap, fmt);

	// Calculate width by parsing fmt for % format specifiers (basic implementation)
	u32 len = 0;
	const char *p = fmt;
	while (*p && *p != '%') p++; // Skip to first % or end
	len = p - fmt;

	// Add estimated length for format args (rough estimate)
	if (*p == '%') {
		// Each format adds ~10 chars on average
		while (*p) {
			if (*p == '%') {
				p++;
				if (*p == 'd' || *p == 'x' || *p == 'X' || *p == 'p' || *p == 'c' || *p == 's' || *p == 'k' || *p == 'K')
					len += 10;
				else if (*p == '%')
					len += 1;
			}
			p++;
		}
	}

	// Calculate centered x position (use width for landscape horizontal centering)
	u32 x = (gfx_ctxt.width - (len * 16)) / 2;
	if (x > 1279) x = 0; // Safety check

	gfx_con_setpos(x, y);

	// Use the same printf logic as gfx_printf, but already positioned
	va_end(ap);
	va_start(ap, fmt);

	while(*fmt)
	{
		if(*fmt == '%')
		{
			fmt++;
			switch(*fmt)
			{
			case 'c':
				gfx_putc(va_arg(ap, u32));
				break;
			case 's':
				gfx_puts(va_arg(ap, char *));
				break;
			case 'd':
				_gfx_putn(va_arg(ap, u32), 10, 0, 0);
				break;
			case 'p':
			case 'P':
			case 'x':
			case 'X':
				_gfx_putn(va_arg(ap, u32), 16, 0, 0);
				break;
			case 'k':
				gfx_con.fgcol = va_arg(ap, u32);
				break;
			case 'K':
				gfx_con.bgcol = va_arg(ap, u32);
				gfx_con.fillbg = 1;
				break;
			case '%':
				gfx_putc('%');
				break;
			case '\0':
				goto out;
			default:
				gfx_putc('%');
				gfx_putc(*fmt);
				break;
			}
		}
		else
			gfx_putc(*fmt);
		fmt++;
	}

out:
	va_end(ap);
}

void gfx_hexdump(u32 base, const void *buf, u32 len)
{
	if (!gfx_con_init_done || gfx_con.mute)
		return;

	u8 *buff = (u8 *)buf;

	u8 prevFontSize = gfx_con.fntsz;
	gfx_con.fntsz = 8;
	for(u32 i = 0; i < len; i++)
	{
		if(i % 0x10 == 0)
		{
			if(i != 0)
			{
				gfx_puts("| ");
				for(u32 j = 0; j < 0x10; j++)
				{
					u8 c = buff[i - 0x10 + j];
					if(c >= 32 && c <= 126)
						gfx_putc(c);
					else
						gfx_putc('.');
				}
				gfx_putc('\n');
			}
			gfx_printf("%08x: ", base + i);
		}
		gfx_printf("%02x ", buff[i]);
		if (i == len - 1)
		{
			int ln = len % 0x10 != 0;
			u32 k = 0x10 - 1;
			if (ln)
			{
				k = (len & 0xF) - 1;
				for (u32 j = 0; j < 0x10 - k; j++)
					gfx_puts("   ");
			}
			gfx_puts("| ");
			for(u32 j = 0; j < (ln ? k : k + 1); j++)
			{
				u8 c = buff[i - k + j];
				if(c >= 32 && c <= 126)
					gfx_putc(c);
				else
					gfx_putc('.');
			}
			gfx_putc('\n');
		}
	}
	gfx_putc('\n');
	gfx_con.fntsz = prevFontSize;
}

void gfx_hexdiff(u32 base, const void *buf1, const void *buf2, u32 len)
{
	if (!gfx_con_init_done || gfx_con.mute)
		return;

	u8 *buff1 = (u8 *)buf1;
	u8 *buff2 = (u8 *)buf2;

	if (memcmp(buff1, buff2, len) == 0)
	{
		gfx_printf("Diff: No differences found.\n");
		return;
	}

	u8 prevFontSize = gfx_con.fntsz;
	gfx_con.fntsz = 8;
	for(u32 i = 0; i < len; i+=0x10)
	{
		u32 bytes_left = len - i < 0x10 ? len - i : 0x10;
		if (memcmp(buff1 + i, buff2 + i, bytes_left) == 0)
			continue;
		gfx_printf("Diff 1: %08x: ", base + i);
		for (u32 j = 0; j < bytes_left; j++)
		{
			if (buff1[i+j] != buff2[i+j])
				gfx_con.fgcol = COLOR_ORANGE;
			gfx_printf("%02x ", buff1[i+j]);
			gfx_con.fgcol = 0xFFCCCCCC;
		}
		gfx_puts("| ");
		gfx_putc('\n');
		gfx_printf("Diff 2: %08x: ", base + i);
		for (u32 j = 0; j < bytes_left; j++)
		{
			if (buff1[i+j] != buff2[i+j])
				gfx_con.fgcol = COLOR_ORANGE;
			gfx_printf("%02x ", buff2[i+j]);
			gfx_con.fgcol = 0xFFCCCCCC;
		}
		gfx_puts("| ");
		gfx_putc('\n');
		gfx_putc('\n');
	}
	gfx_putc('\n');
	gfx_con.fntsz = prevFontSize;
}

static int abs(int x)
{
	if (x < 0)
		return -x;
	return x;
}

void gfx_set_pixel(u32 x, u32 y, u32 color)
{
	// Transform for landscape mode: external (x,y) → internal (y, 1279-x)
	gfx_ctxt.fb[y + (1279 - x) * gfx_ctxt.stride] = color;
}

void gfx_line(int x0, int y0, int x1, int y1, u32 color)
{
	int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
	int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
	int err = (dx > dy ? dx : -dy) / 2, e2;

	while (1)
	{
		// Transform for landscape mode
		gfx_set_pixel(x0, y0, color);
		if (x0 == x1 && y0 == y1)
			break;
		e2 = err;
		if (e2 >-dx)
		{
			err -= dy;
			x0 += sx;
		}
		if (e2 < dy)
		{
			err += dx;
			y0 += sy;
		}
	}
}

void gfx_set_rect_grey(const u8 *buf, u32 size_x, u32 size_y, u32 pos_x, u32 pos_y)
{
	u32 pos = 0;
	for (u32 y = pos_y; y < (pos_y + size_y); y++)
	{
		for (u32 x = pos_x; x < (pos_x + size_x); x++)
		{
			memset(&gfx_ctxt.fb[x + y*gfx_ctxt.stride], buf[pos], 4);
			pos++;
		}
	}
}


void gfx_set_rect_rgb(const u8 *buf, u32 size_x, u32 size_y, u32 pos_x, u32 pos_y)
{
	u32 pos = 0;
	for (u32 y = pos_y; y < (pos_y + size_y); y++)
	{
		for (u32 x = pos_x; x < (pos_x + size_x); x++)
		{
			gfx_ctxt.fb[x + y * gfx_ctxt.stride] = buf[pos + 2] | (buf[pos + 1] << 8) | (buf[pos] << 16);
			pos+=3;
		}
	}
}

void gfx_set_rect_argb(const u32 *buf, u32 size_x, u32 size_y, u32 pos_x, u32 pos_y)
{
	u32 *ptr = (u32 *)buf;
	for (u32 y = pos_y; y < (pos_y + size_y); y++)
		for (u32 x = pos_x; x < (pos_x + size_x); x++)
			gfx_ctxt.fb[x + y * gfx_ctxt.stride] = *ptr++;
}

void gfx_render_bmp_argb(const u32 *buf, u32 size_x, u32 size_y, u32 pos_x, u32 pos_y)
{
	for (u32 y = pos_y; y < (pos_y + size_y); y++)
	{
		for (u32 x = pos_x; x < (pos_x + size_x); x++)
			gfx_ctxt.fb[x + y * gfx_ctxt.stride] = buf[(size_y + pos_y - 1 - y ) * size_x + x - pos_x];
	}
}

void gfx_draw_title_bar(const char *title)
{
	// Save current graphics state
	u8 saved_fillbg = gfx_con.fillbg;
	u32 saved_bgcol = gfx_con.bgcol;

	// Draw TOP BAR (16px tall, dark grey background for dark theme)
	for (u32 y = 0; y < 1280; y++)
	{
		for (u32 x = 0; x < 16; x++)
		{
			gfx_ctxt.fb[x + y * gfx_ctxt.stride] = 0xFF3D3D3D;
		}
	}

	// Draw title in top-left corner (cyan text on dark grey)
	gfx_con_setcol(0xFF00D8FF, 1, 0xFF3D3D3D);
	gfx_con_setpos(0, 0);
	gfx_printf("%s", title);

	// Restore graphics state (preserve saved fillbg, only set fgcol)
	gfx_con.fillbg = saved_fillbg;
	gfx_con.bgcol = saved_bgcol;
	gfx_con.fgcol = 0xFFCCCCCC;
}

void gfx_draw_bottom_bar(const char *legend)
{
	// Save current graphics state
	u8 saved_fillbg = gfx_con.fillbg;
	u32 saved_bgcol = gfx_con.bgcol;

	// Draw BOTTOM BAR (16px tall, dark grey background for dark theme)
	for (u32 y = 0; y < 1280; y++)
	{
		for (u32 x = 704; x < 720; x++)
		{
			gfx_ctxt.fb[x + y * gfx_ctxt.stride] = 0xFF3D3D3D;
		}
	}

	// Draw legend in bottom-left corner (cyan text on dark grey)
	gfx_con_setcol(0xFF00D8FF, 1, 0xFF3D3D3D);
	gfx_con_setpos(0, 704);
	gfx_printf("%s", legend);

	// Restore graphics state (preserve saved fillbg, only set fgcol)
	gfx_con.fillbg = saved_fillbg;
	gfx_con.bgcol = saved_bgcol;
	gfx_con.fgcol = 0xFFCCCCCC;
}
