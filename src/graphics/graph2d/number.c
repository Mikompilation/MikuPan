#include "common.h"
#include "typedefs.h"
#include "number.h"

#include "outgame/mode_slct.h"

// #include "graphics/graph2d/message.h" // DO NOT IMPORT!!
// #include "graphics/graph2d/tim2.h" // DO NOT IMPORT!!

#include "message.h"
#include "tim2.h"

SPRT_DAT number_tex[][10] = {
{    {
        .tex0 = 2307268001604444928,
        .u = 264,
        .v = 48,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 288,
        .v = 48,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 312,
        .v = 48,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 336,
        .v = 48,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 360,
        .v = 48,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 384,
        .v = 48,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 408,
        .v = 48,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 432,
        .v = 48,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 456,
        .v = 48,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 480,
        .v = 48,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    }},{    {
        .tex0 = 2306890457804774597,
        .u = 0,
        .v = 0,
        .w = 24,
        .h = 40,
        .x = 99,
        .y = 384,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2306890457804774597,
        .u = 24,
        .v = 0,
        .w = 24,
        .h = 40,
        .x = 99,
        .y = 384,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2306890457804774597,
        .u = 48,
        .v = 0,
        .w = 24,
        .h = 40,
        .x = 99,
        .y = 384,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2306890457804774597,
        .u = 72,
        .v = 0,
        .w = 24,
        .h = 40,
        .x = 99,
        .y = 384,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2306890457804774597,
        .u = 96,
        .v = 0,
        .w = 24,
        .h = 40,
        .x = 99,
        .y = 384,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2306890457804774597,
        .u = 0,
        .v = 40,
        .w = 24,
        .h = 40,
        .x = 99,
        .y = 384,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2306890457804774597,
        .u = 24,
        .v = 40,
        .w = 24,
        .h = 40,
        .x = 99,
        .y = 384,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2306890457804774597,
        .u = 48,
        .v = 40,
        .w = 24,
        .h = 40,
        .x = 99,
        .y = 384,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2306890457804774597,
        .u = 72,
        .v = 40,
        .w = 24,
        .h = 40,
        .x = 99,
        .y = 384,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2306890457804774597,
        .u = 96,
        .v = 40,
        .w = 24,
        .h = 40,
        .x = 99,
        .y = 384,
        .pri = 4096,
        .alpha = 128,
    }},{    {
        .tex0 = 2307254806255806464,
        .u = 0,
        .v = 0,
        .w = 24,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307254806255806464,
        .u = 24,
        .v = 0,
        .w = 24,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307254806255806464,
        .u = 48,
        .v = 0,
        .w = 24,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307254806255806464,
        .u = 72,
        .v = 0,
        .w = 24,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307254806255806464,
        .u = 96,
        .v = 0,
        .w = 24,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307254806255806464,
        .u = 0,
        .v = 28,
        .w = 24,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307254806255806464,
        .u = 24,
        .v = 28,
        .w = 24,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307254806255806464,
        .u = 48,
        .v = 28,
        .w = 24,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307254806255806464,
        .u = 72,
        .v = 28,
        .w = 24,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307254806255806464,
        .u = 96,
        .v = 28,
        .w = 24,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    }},{    {
        .tex0 = 2307268001604444928,
        .u = 0,
        .v = 72,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 24,
        .v = 72,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 48,
        .v = 72,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 72,
        .v = 72,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 96,
        .v = 72,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 120,
        .v = 72,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 144,
        .v = 72,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 168,
        .v = 72,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 192,
        .v = 72,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    },    {
        .tex0 = 2307268001604444928,
        .u = 216,
        .v = 72,
        .w = 24,
        .h = 24,
        .x = 320,
        .y = 224,
        .pri = 4096,
        .alpha = 128,
    }},};



void NumberDisp(int number, short int pos_x, short int pos_y, u_char font, u_char r, u_char g, u_char b, u_char alpha, int pri, int digit, int type)
{
	int i;
	int multi10;
	int multi10_bak;
	int disp_number;
	int disp_digit;

    multi10 = 1;

    for (i = 0; i < digit; i++)
    {
        multi10 *= 10;
        multi10_bak = multi10;
    }

    disp_number = number % multi10;

    if (disp_number != 0)
    {
        for (i = 0; i < digit; i++)
        {
            multi10 /= 10;

            if (disp_number / multi10 != 0)
            {
                disp_digit = digit - i;
                break;
            }
        }
    }
    else
    {
        disp_digit = 1;
    }

    multi10 = multi10_bak;

    for (i = 0; i < digit; i++)
    {
        multi10 /= 10;

        switch (type)
        {
        case 0:
            if (digit - disp_digit <= i)
            {
                NumberFontDisp(disp_number / multi10, i, pos_x, pos_y, font, r, g, b, pri, alpha);
                disp_number -= (disp_number / multi10) * multi10;
            }
        break;
        case 1:
            if (digit - disp_digit <= i)
            {
                NumberFontDisp(disp_number / multi10, i - (digit - disp_digit), pos_x, pos_y, font, r, g, b, pri, alpha);
                disp_number -= (disp_number / multi10) * multi10;
            }
        break;
        case 2:
            NumberFontDisp(disp_number / multi10, i, pos_x, pos_y, font, r, g, b, pri, alpha);
            disp_number -= (disp_number / multi10) * multi10;
        break;
        }
    }
}

void NumberFontDisp(u_char number, u_char no, short int pos_x, short int pos_y, u_char font, u_char r, u_char g, u_char b, int pri, u_char alpha)
{
	u_short font_w[5] = { 0x18, 0x15, 0x18, 0x18, 0x18 };
	DISP_SPRT ds;

    if (font == 0 || font == 3)
    {
        SetInteger3(pri, font_w[font] * no + pos_x, pos_y, 0, r, g, b, alpha, number);
    }
    else
    {
        CopySprDToSpr(&ds, &number_tex[font][number]);

        ds.x = font_w[font] * no + pos_x;
        ds.y = pos_y;
        ds.pri = pri;
        ds.r = r;
        ds.g = g;
        ds.b = b;
        ds.z = 0xfffffff - pri;
        ds.alpha = alpha;

        DispSprD(&ds);
    }
}
