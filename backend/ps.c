/* ps.c - Post Script output */
/*
    libzint - the open source barcode library
    Copyright (C) 2009-2023 Robin Stuart <rstuart114@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the project nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
 */
/* SPDX-License-Identifier: BSD-3-Clause */

#include <errno.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include "common.h"
#include "output.h"

static void colour_to_pscolor(int option, int colour, char *output) {
    *output = '\0';
    if ((option & CMYK_COLOUR) == 0) {
        /* Use RGB colour space */
        switch (colour) {
            case 1: /* Cyan */
                strcat(output, "0.00 1.00 1.00");
                break;
            case 2: /* Blue */
                strcat(output, "0.00 0.00 1.00");
                break;
            case 3: /* Magenta */
                strcat(output, "1.00 0.00 1.00");
                break;
            case 4: /* Red */
                strcat(output, "1.00 0.00 0.00");
                break;
            case 5: /* Yellow */
                strcat(output, "1.00 1.00 0.00");
                break;
            case 6: /* Green */
                strcat(output, "0.00 1.00 0.00");
                break;
            case 8: /* White */
                strcat(output, "1.00 1.00 1.00");
                break;
            default: /* Black */
                strcat(output, "0.00 0.00 0.00");
                break;
        }
        strcat(output, " setrgbcolor");
    } else {
        /* Use CMYK colour space */
        switch (colour) {
            case 1: /* Cyan */
                strcat(output, "1.00 0.00 0.00 0.00");
                break;
            case 2: /* Blue */
                strcat(output, "1.00 1.00 0.00 0.00");
                break;
            case 3: /* Magenta */
                strcat(output, "0.00 1.00 0.00 0.00");
                break;
            case 4: /* Red */
                strcat(output, "0.00 1.00 1.00 0.00");
                break;
            case 5: /* Yellow */
                strcat(output, "0.00 0.00 1.00 0.00");
                break;
            case 6: /* Green */
                strcat(output, "1.00 0.00 1.00 0.00");
                break;
            case 8: /* White */
                strcat(output, "0.00 0.00 0.00 0.00");
                break;
            default: /* Black */
                strcat(output, "0.00 0.00 0.00 1.00");
                break;
        }
        strcat(output, " setcmykcolor");
    }
}

static void ps_convert(const unsigned char *string, unsigned char *ps_string) {
    const unsigned char *s;
    unsigned char *p = ps_string;

    for (s = string; *s; s++) {
        switch (*s) {
            case '(':
            case ')':
            case '\\':
                *p++ = '\\';
                *p++ = *s;
                break;
            case 0xC2: /* See `to_iso8859_1()` in raster.c */
                *p++ = *++s;
                break;
            case 0xC3:
                *p++ = *++s + 64;
                break;
            default:
                if (*s < 0x80) {
                    *p++ = *s;
                }
                break;

        }
    }
    *p = '\0';
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL void ps_convert_test(const unsigned char *string, unsigned char *ps_string) {
	ps_convert(string, ps_string);
}
#endif

INTERNAL int ps_plot(struct zint_symbol *symbol) {
    FILE *feps;
    unsigned char fgred, fggrn, fgblu, bgred, bggrn, bgblu, bgalpha;
    int fgcyan, fgmagenta, fgyellow, fgblack, bgcyan, bgmagenta, bgyellow, bgblack;
    float red_ink = 0.0f, green_ink = 0.0f, blue_ink = 0.0f; /* Suppress `-Wmaybe-uninitialized` */
    float red_paper = 0.0f, green_paper = 0.0f, blue_paper = 0.0f;
    float cyan_ink = 0.0f, magenta_ink = 0.0f, yellow_ink = 0.0f, black_ink = 0.0f;
    float cyan_paper = 0.0f, magenta_paper = 0.0f, yellow_paper = 0.0f, black_paper = 0.0f;
    int error_number = 0;
    float ax, ay, bx, by, cx, cy, dx, dy, ex, ey, fx, fy;
    float previous_diameter;
    float radius, half_radius, half_sqrt3_radius;
    int colour_index, colour_rect_flag;
    char ps_color[33]; /* max "1.00 0.00 0.00 0.00 setcmykcolor" = 32 + 1 */
    int draw_background = 1;
    struct zint_vector_rect *rect;
    struct zint_vector_hexagon *hex;
    struct zint_vector_circle *circle;
    struct zint_vector_string *string;
    const char *locale = NULL;
    const char *font;
    int i, len;
    int ps_len = 0;
    int iso_latin1 = 0;
    int have_circles_with_width = 0, have_circles_without_width = 0;
    const int output_to_stdout = symbol->output_options & BARCODE_STDOUT;
    unsigned char *ps_string;

    if (symbol->vector == NULL) {
        strcpy(symbol->errtxt, "646: Vector header NULL");
        return ZINT_ERROR_INVALID_DATA;
    }

    if (output_to_stdout) {
        feps = stdout;
    } else {
        if (!(feps = out_fopen(symbol->outfile, "w"))) {
            sprintf(symbol->errtxt, "645: Could not open output file (%d: %.30s)", errno, strerror(errno));
            return ZINT_ERROR_FILE_ACCESS;
        }
    }

    locale = setlocale(LC_ALL, "C");

    if ((symbol->output_options & CMYK_COLOUR) == 0) {
        (void) out_colour_get_rgb(symbol->fgcolour, &fgred, &fggrn, &fgblu, NULL /*alpha*/);
        red_ink = fgred / 255.0f;
        green_ink = fggrn / 255.0f;
        blue_ink = fgblu / 255.0f;

        (void) out_colour_get_rgb(symbol->bgcolour, &bgred, &bggrn, &bgblu, &bgalpha);
        red_paper = bgred / 255.0f;
        green_paper = bggrn / 255.0f;
        blue_paper = bgblu / 255.0f;
    } else {
        (void) out_colour_get_cmyk(symbol->fgcolour, &fgcyan, &fgmagenta, &fgyellow, &fgblack, NULL /*rgb_alpha*/);
        cyan_ink = fgcyan / 100.0f;
        magenta_ink = fgmagenta / 100.0f;
        yellow_ink = fgyellow / 100.0f;
        black_ink = fgblack / 100.0f;

        (void) out_colour_get_cmyk(symbol->bgcolour, &bgcyan, &bgmagenta, &bgyellow, &bgblack, &bgalpha);
        cyan_paper = bgcyan / 100.0f;
        magenta_paper = bgmagenta / 100.0f;
        yellow_paper = bgyellow / 100.0f;
        black_paper = bgblack / 100.0f;
    }
    if (bgalpha == 0) {
        draw_background = 0;
    }

    for (i = 0, len = (int) ustrlen(symbol->text); i < len; i++) {
        switch (symbol->text[i]) {
            case '(':
            case ')':
            case '\\':
                ps_len += 2;
                break;
            default:
                if (!iso_latin1 && symbol->text[i] >= 0x80) {
                    iso_latin1 = 1;
                }
                ps_len++; /* Will overcount 2 byte UTF-8 chars */
                break;
        }
    }

    ps_string = (unsigned char *) z_alloca(ps_len + 1);

    /* Check for circle widths */
    for (circle = symbol->vector->circles; circle; circle = circle->next) {
        if (circle->width) {
            have_circles_with_width = 1;
        } else {
            have_circles_without_width = 1;
        }
    }

    /* Start writing the header */
    fputs("%!PS-Adobe-3.0 EPSF-3.0\n", feps);
    if (ZINT_VERSION_BUILD) {
        fprintf(feps, "%%%%Creator: Zint %d.%d.%d.%d\n",
                ZINT_VERSION_MAJOR, ZINT_VERSION_MINOR, ZINT_VERSION_RELEASE, ZINT_VERSION_BUILD);
    } else {
        fprintf(feps, "%%%%Creator: Zint %d.%d.%d\n", ZINT_VERSION_MAJOR, ZINT_VERSION_MINOR, ZINT_VERSION_RELEASE);
    }
    fputs("%%Title: Zint Generated Symbol\n"
          "%%Pages: 0\n", feps);
    fprintf(feps, "%%%%BoundingBox: 0 0 %d %d\n",
            (int) ceilf(symbol->vector->width), (int) ceilf(symbol->vector->height));
    fputs("%%EndComments\n", feps);

    /* Definitions */
    if (have_circles_without_width) {
        /* Disc: x y radius TD */
        fputs("/TD { newpath 0 360 arc fill } bind def\n", feps);
    }
    if (have_circles_with_width) {
        /* Circle (ring): x y radius width TC (adapted from BWIPP renmaxicode.ps) */
        fputs("/TC { newpath 4 1 roll 3 copy 0 360 arc closepath 4 -1 roll add 360 0 arcn closepath fill }"
                " bind def\n", feps);
    }
    if (symbol->vector->hexagons) {
        fputs("/TH { 0 setlinewidth moveto lineto lineto lineto lineto lineto closepath fill } bind def\n", feps);
    }
    fputs("/TB { 2 copy } bind def\n"
          "/TR { newpath 4 1 roll exch moveto 1 index 0 rlineto 0 exch rlineto neg 0 rlineto closepath fill }"
            " bind def\n"
          "/TE { pop pop } bind def\n", feps);

    fputs("newpath\n", feps);

    /* Now the actual representation */

    /* Background */
    if (draw_background) {
        if ((symbol->output_options & CMYK_COLOUR) == 0) {
            fprintf(feps, "%.2f %.2f %.2f setrgbcolor\n", red_paper, green_paper, blue_paper);
        } else {
            fprintf(feps, "%.2f %.2f %.2f %.2f setcmykcolor\n", cyan_paper, magenta_paper, yellow_paper, black_paper);
        }

        fprintf(feps, "%.2f 0.00 TB 0.00 %.2f TR\n", symbol->vector->height, symbol->vector->width);
        fputs("TE\n", feps);
    }

    if (symbol->symbology != BARCODE_ULTRA) {
        if ((symbol->output_options & CMYK_COLOUR) == 0) {
            fprintf(feps, "%.2f %.2f %.2f setrgbcolor\n", red_ink, green_ink, blue_ink);
        } else {
            fprintf(feps, "%.2f %.2f %.2f %.2f setcmykcolor\n", cyan_ink, magenta_ink, yellow_ink, black_ink);
        }
    }

    /* Rectangles */
    if (symbol->symbology == BARCODE_ULTRA) {
        colour_rect_flag = 0;
        rect = symbol->vector->rectangles;
        while (rect) {
            if (rect->colour == -1) { /* Foreground */
                if (colour_rect_flag == 0) {
                    /* Set foreground colour */
                    if ((symbol->output_options & CMYK_COLOUR) == 0) {
                        fprintf(feps, "%.2f %.2f %.2f setrgbcolor\n", red_ink, green_ink, blue_ink);
                    } else {
                        fprintf(feps, "%.2f %.2f %.2f %.2f setcmykcolor\n",
                                cyan_ink, magenta_ink, yellow_ink, black_ink);
                    }
                    colour_rect_flag = 1;
                }
                fprintf(feps, "%.2f %.2f TB %.2f %.2f TR\n",
                        rect->height, (symbol->vector->height - rect->y) - rect->height, rect->x, rect->width);
                fputs("TE\n", feps);
            }
            rect = rect->next;
        }
        for (colour_index = 1; colour_index <= 8; colour_index++) {
            colour_rect_flag = 0;
            rect = symbol->vector->rectangles;
            while (rect) {
                if (rect->colour == colour_index) {
                    if (colour_rect_flag == 0) {
                        /* Set new colour */
                        colour_to_pscolor(symbol->output_options, colour_index, ps_color);
                        fprintf(feps, "%s\n", ps_color);
                        colour_rect_flag = 1;
                    }
                    fprintf(feps, "%.2f %.2f TB %.2f %.2f TR\n",
                            rect->height, (symbol->vector->height - rect->y) - rect->height, rect->x, rect->width);
                    fputs("TE\n", feps);
                }
                rect = rect->next;
            }
        }
    } else {
        rect = symbol->vector->rectangles;
        while (rect) {
            fprintf(feps, "%.2f %.2f TB %.2f %.2f TR\n",
                    rect->height, (symbol->vector->height - rect->y) - rect->height, rect->x, rect->width);
            fputs("TE\n", feps);
            rect = rect->next;
        }
    }

    /* Hexagons */
    previous_diameter = radius = half_radius = half_sqrt3_radius = 0.0f;
    hex = symbol->vector->hexagons;
    while (hex) {
        if (previous_diameter != hex->diameter) {
            previous_diameter = hex->diameter;
            radius = (float) (0.5 * previous_diameter);
            half_radius = (float) (0.25 * previous_diameter);
            half_sqrt3_radius = (float) (0.43301270189221932338 * previous_diameter);
        }
        if ((hex->rotation == 0) || (hex->rotation == 180)) {
            ay = (symbol->vector->height - hex->y) + radius;
            by = (symbol->vector->height - hex->y) + half_radius;
            cy = (symbol->vector->height - hex->y) - half_radius;
            dy = (symbol->vector->height - hex->y) - radius;
            ey = (symbol->vector->height - hex->y) - half_radius;
            fy = (symbol->vector->height - hex->y) + half_radius;
            ax = hex->x;
            bx = hex->x + half_sqrt3_radius;
            cx = hex->x + half_sqrt3_radius;
            dx = hex->x;
            ex = hex->x - half_sqrt3_radius;
            fx = hex->x - half_sqrt3_radius;
        } else {
            ay = (symbol->vector->height - hex->y);
            by = (symbol->vector->height - hex->y) + half_sqrt3_radius;
            cy = (symbol->vector->height - hex->y) + half_sqrt3_radius;
            dy = (symbol->vector->height - hex->y);
            ey = (symbol->vector->height - hex->y) - half_sqrt3_radius;
            fy = (symbol->vector->height - hex->y) - half_sqrt3_radius;
            ax = hex->x - radius;
            bx = hex->x - half_radius;
            cx = hex->x + half_radius;
            dx = hex->x + radius;
            ex = hex->x + half_radius;
            fx = hex->x - half_radius;
        }
        fprintf(feps, "%.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f TH\n",
                ax, ay, bx, by, cx, cy, dx, dy, ex, ey, fx, fy);
        hex = hex->next;
    }

    /* Circles */
    previous_diameter = radius = 0.0f;
    circle = symbol->vector->circles;
    while (circle) {
        if (previous_diameter != circle->diameter - circle->width) {
            previous_diameter = circle->diameter - circle->width;
            radius = (float) (0.5 * previous_diameter);
        }
        if (circle->colour) { /* Legacy - no longer used */
            /* A 'white' circle */
            if ((symbol->output_options & CMYK_COLOUR) == 0) {
                fprintf(feps, "%.2f %.2f %.2f setrgbcolor\n", red_paper, green_paper, blue_paper);
            } else {
                fprintf(feps, "%.2f %.2f %.2f %.2f setcmykcolor\n",
                        cyan_paper, magenta_paper, yellow_paper, black_paper);
            }
            if (circle->width) {
                fprintf(feps, "%.2f %.2f %.3f %.3f TC\n",
                        circle->x, (symbol->vector->height - circle->y), radius, circle->width);
            } else {
                fprintf(feps, "%.2f %.2f %.2f TD\n", circle->x, (symbol->vector->height - circle->y), radius);
            }
            if (circle->next) {
                if ((symbol->output_options & CMYK_COLOUR) == 0) {
                    fprintf(feps, "%.2f %.2f %.2f setrgbcolor\n", red_ink, green_ink, blue_ink);
                } else {
                    fprintf(feps, "%.2f %.2f %.2f %.2f setcmykcolor\n", cyan_ink, magenta_ink, yellow_ink, black_ink);
                }
            }
        } else {
            /* A 'black' circle */
            if (circle->width) {
                fprintf(feps, "%.2f %.2f %.3f %.3f TC\n",
                        circle->x, (symbol->vector->height - circle->y), radius, circle->width);
            } else {
                fprintf(feps, "%.2f %.2f %.2f TD\n", circle->x, (symbol->vector->height - circle->y), radius);
            }
        }
        circle = circle->next;
    }

    /* Text */

    string = symbol->vector->strings;

    if (string) {
        if ((symbol->output_options & BOLD_TEXT) && !is_extendable(symbol->symbology)) {
            font = "Helvetica-Bold";
        } else {
            font = "Helvetica";
        }
        if (iso_latin1) {
            /* Change encoding to ISO 8859-1, see Postscript Language Reference Manual 2nd Edition Example 5.6 */
            fprintf(feps, "/%s findfont\n", font);
            fputs("dup length dict begin\n"
                  "{1 index /FID ne {def} {pop pop} ifelse} forall\n"
                  "/Encoding ISOLatin1Encoding def\n"
                  "currentdict\n"
                  "end\n"
                  "/Helvetica-ISOLatin1 exch definefont pop\n", feps);
            font = "Helvetica-ISOLatin1";
        }
        do {
            ps_convert(string->text, ps_string);
            fputs("matrix currentmatrix\n", feps);
            fprintf(feps, "/%s findfont\n", font);
            fprintf(feps, "%.2f scalefont setfont\n", string->fsize);
            fprintf(feps, " 0 0 moveto %.2f %.2f translate 0.00 rotate 0 0 moveto\n",
                    string->x, (symbol->vector->height - string->y));
            if (string->halign == 0 || string->halign == 2) { /* Need width for middle or right align */
                fprintf(feps, " (%s) stringwidth\n", ps_string);
            }
            if (string->rotation != 0) {
                fputs("gsave\n", feps);
                fprintf(feps, "%d rotate\n", 360 - string->rotation);
            }
            if (string->halign == 0 || string->halign == 2) {
                fputs("pop\n", feps);
                fprintf(feps, "%s 0 rmoveto\n", string->halign == 2 ? "neg" : "-2 div");
            }
            fprintf(feps, " (%s) show\n", ps_string);
            if (string->rotation != 0) {
                fputs("grestore\n", feps);
            }
            fputs("setmatrix\n", feps);
            string = string->next;
        } while (string);
    }

    if (locale)
        setlocale(LC_ALL, locale);

    if (ferror(feps)) {
        sprintf(symbol->errtxt, "647: Incomplete write to output (%d: %.30s)", errno, strerror(errno));
        if (!output_to_stdout) {
            (void) fclose(feps);
        }
        return ZINT_ERROR_FILE_WRITE;
    }

    if (output_to_stdout) {
        if (fflush(feps) != 0) {
            sprintf(symbol->errtxt, "648: Incomplete flush to output (%d: %.30s)", errno, strerror(errno));
            return ZINT_ERROR_FILE_WRITE;
        }
    } else {
        if (fclose(feps) != 0) {
            sprintf(symbol->errtxt, "649: Failure on closing output file (%d: %.30s)", errno, strerror(errno));
            return ZINT_ERROR_FILE_WRITE;
        }
    }

    return error_number;
}

/* vim: set ts=4 sw=4 et : */
