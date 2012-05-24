/* cdok -- Calcudoku solver/generator
 * Copyright (C) 2012 Daniel Beer <dlbeer@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef PRINTER_H_
#define PRINTER_H_

#include <stdio.h>
#include "cdok.h"

/* This structure specifies Unicode characters for the top and bottom
 * borders of the puzzle:
 *
 *    start:     left corner
 *    end:       right corner
 *    tee_major: connection with a major vertical grid-line
 *    tee_minor: connection with a minor vertical grid-line
 */
struct cdok_template_hborder {
	uint16_t	start;
	uint16_t	end;
	uint16_t	tee_major;
	uint16_t	tee_minor;
};

/* This structure specifies Unicode characters for drawing a puzzle grid:
 *
 *     top:             characters for the top border
 *     bottom:          characters for the bottom border
 *     hline_major:     major horizontal gridlines
 *     hline_minor:     minor horizontal gridlines
 *     vline_major:     major vertical gridlines
 *     vline_minor:     minor vertical gridlines
 *     tee_left_major:  connection on left with a major horizontal gridline
 *     tee_left_minor:  connection on left with a minor horizontal gridline
 *     tee_right_major: connection on right with a major horizontal gridline
 *     tee_right_minor: connection on right with a minor horizontal gridline
 *     inners:          characters used at the boundary of four cells
 */
struct cdok_template {
	struct cdok_template_hborder	top;
	struct cdok_template_hborder	bottom;
	uint16_t			hline_major;
	uint16_t			hline_minor;
	uint16_t			vline_major;
	uint16_t			vline_minor;
	uint16_t			tee_left_major;
	uint16_t			tee_left_minor;
	uint16_t			tee_right_major;
	uint16_t			tee_right_minor;
	uint16_t			inners[16];
};

/* Within the grid, different characters are used to connect gridlines
 * depending on whether the lines to the top, left, right or bottom are
 * major or minor. This enum gives the appropriate indices into the
 * "inners" field of the template for each case.
 *
 * For example CDOK_TIN_TR is the index of the character used when the
 * gridlines to the top and to the right of the character are major, and
 * the other two are minor.
 *
 * Note that not all cases are possible -- it's not possibe for there to
 * be only a single major gridline from an inner character.
 */
typedef enum {
	CDOK_TIN_0	= 0,
	CDOK_TIN_L	= 1,
	CDOK_TIN_R	= 2,
	CDOK_TIN_LR	= 3,
	CDOK_TIN_T	= 4,
	CDOK_TIN_TL	= 5,
	CDOK_TIN_TR	= 6,
	CDOK_TIN_TLR	= 7,
	CDOK_TIN_B	= 8,
	CDOK_TIN_BL	= 9,
	CDOK_TIN_BR	= 10,
	CDOK_TIN_BLR	= 11,
	CDOK_TIN_BT	= 12,
	CDOK_TIN_BTL	= 13,
	CDOK_TIN_BTR	= 14,
	CDOK_TIN_BTLR	= 15
} cdok_template_inner_t;

/* ASCII template. */
extern const struct cdok_template cdok_template_ascii;

/* Template using Unicode line-drawing characters. */
extern const struct cdok_template cdok_template_unicode;

/* Print a puzzle structure with the given values filled in, using the
 * specified template.
 */
void cdok_format_puzzle(const struct cdok_template *templ,
			const struct cdok_puzzle *puz,
			const uint8_t *values,
			FILE *out);

/* Print a puzzle spec that can be read by the parser. */
void cdok_print_puzzle(const struct cdok_puzzle *puz,
		       const uint8_t *values, FILE *out);

#endif
