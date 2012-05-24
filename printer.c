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

#include "cdok.h"
#include "printer.h"

/* Print a puzzle spec that can be read by the parser. */
void cdok_print_puzzle(const struct cdok_puzzle *puz,
		       const uint8_t *values, FILE *out)
{
	int y;

	for (y = 0; y < puz->size; y++) {
		int x;

		for (x = 0; x < puz->size; x++) {
			cdok_pos_t c = CDOK_POS(x, y);

			if (x)
				fprintf(out, "\t");

			if (values[c]) {
				fprintf(out, "%d", values[c]);
			} else if (puz->group_map[c] != CDOK_GROUP_NONE) {
				const struct cdok_group *g =
					&puz->groups[puz->group_map[c]];

				fprintf(out, "%c",
					cdok_group_to_char(puz->group_map[c]));
				if (g->members[0] == c)
					fprintf(out, "%c%d",
						g->type, g->target);
			}
		}

		fprintf(out, "\n");
	}
}

const struct cdok_template cdok_template_unicode = {
	.top = {
		.start		= 0x2554,
		.end		= 0x2557,
		.tee_major	= 0x2566,
		.tee_minor	= 0x2550
	},
	.bottom = {
		.start		= 0x255a,
		.end		= 0x255d,
		.tee_major	= 0x2569,
		.tee_minor	= 0x2550
	},
	.hline_major		= 0x2550,
	.hline_minor		= 0x2508,
	.vline_major		= 0x2551,
	.vline_minor		= 0x250a,
	.tee_left_major		= 0x2560,
	.tee_left_minor		= 0x2551,
	.tee_right_major	= 0x2563,
	.tee_right_minor	= 0x2551,
	.inners = {
		[CDOK_TIN_0]	= ' ',
		[CDOK_TIN_LR]	= 0x2550,
		[CDOK_TIN_TL]	= 0x255d,
		[CDOK_TIN_TR]	= 0x255a,
		[CDOK_TIN_TLR]	= 0x2569,
		[CDOK_TIN_BL]	= 0x2557,
		[CDOK_TIN_BR]	= 0x2554,
		[CDOK_TIN_BLR]	= 0x2566,
		[CDOK_TIN_BT]	= 0x2551,
		[CDOK_TIN_BTL]	= 0x2563,
		[CDOK_TIN_BTR]	= 0x2560,
		[CDOK_TIN_BTLR]	= 0x256c
	}
};

const struct cdok_template cdok_template_ascii = {
	.top = {
		.start		= '+',
		.end		= '+',
		.tee_major	= '=',
		.tee_minor	= '='
	},
	.bottom = {
		.start		= '+',
		.end		= '+',
		.tee_major	= '=',
		.tee_minor	= '='
	},
	.hline_major		= '=',
	.hline_minor		= '.',
	.vline_major		= '|',
	.vline_minor		= ':',
	.tee_left_major		= '+',
	.tee_left_minor		= '|',
	.tee_right_major	= '+',
	.tee_right_minor	= '|',
	.inners = {
		[CDOK_TIN_0]	= ' ',
		[CDOK_TIN_LR]	= '=',
		[CDOK_TIN_TL]	= '+',
		[CDOK_TIN_TR]	= '+',
		[CDOK_TIN_TLR]	= '+',
		[CDOK_TIN_BL]	= '+',
		[CDOK_TIN_BR]	= '+',
		[CDOK_TIN_BLR]	= '+',
		[CDOK_TIN_BT]	= '|',
		[CDOK_TIN_BTL]	= '+',
		[CDOK_TIN_BTR]	= '+',
		[CDOK_TIN_BTLR]	= '+'
	}
};

/* Print the given Unicode character <count> times, in UTF-8 */
static void print_char(uint16_t ch, int count, FILE *out)
{
	char text[6];

	if (ch < 0x80) {
		text[0] = ch;
		text[1] = 0;
	} else if (ch < 0x800) {
		text[0] = 0xc0 | (ch >> 6);
		text[1] = 0x80 | (ch & 0x3f);
		text[2] = 0;
	} else {
		text[0] = 0xe0 | (ch >> 12);
		text[1] = 0x80 | ((ch >> 6) & 0x3f);
		text[2] = 0x80 | (ch & 0x3f);
		text[3] = 0;
	}

	while (count > 0) {
		fputs(text, out);
		count--;
	}
}

/* Write a value, if valid, in the given text buffer and return the
 * length.
 */
static int format_value(char *text, int max_len, uint8_t value)
{
	if (!value) {
		text[0] = 0;
		return 0;
	}

	return snprintf(text, max_len, "%d", value);
}

/* Format a clue for a cell, if the given cell is part of a group. Returns
 * the number of characters written to the text buffer.
 */
static int format_clue(char *text, int max_len,
		       const struct cdok_puzzle *puz,
		       cdok_pos_t c)
{
	const struct cdok_group *g;

	if (puz->group_map[c] == CDOK_GROUP_NONE) {
		text[0] = 0;
		return 0;
	}

	g = &puz->groups[puz->group_map[c]];
	if (g->members[0] == c)
		return snprintf(text, max_len, "%d%c", g->target, g->type);

	text[0] = 0;
	return 0;
}

/* For the given row, return a bitmask where bit N is set if (N, y) is
 * in the same group as (N+1, y). This bitmask tells us how to render
 * vertical gridlines.
 */
static cdok_set_t format_hjoins(const struct cdok_puzzle *puz, int y)
{
	int x;
	cdok_set_t out = 0;

	for (x = 0; x + 1 < puz->size; x++) {
		const uint8_t c = puz->group_map[CDOK_POS(x, y)];
		const uint8_t right = puz->group_map[CDOK_POS(x + 1, y)];

		if (c != CDOK_GROUP_NONE && c == right)
			out |= 1 << x;
	}

	return out;
}

/* For the given row, return a bitmask where bit N is set if (N, y) is
 * in the same group as (N, y+1). This bitmask tells us how to render
 * horizontal gridlines.
 */
static cdok_set_t format_vjoins(const struct cdok_puzzle *puz, int y)
{
	int x;
	cdok_set_t out = 0;

	for (x = 0; x < puz->size; x++) {
		uint8_t c = puz->group_map[CDOK_POS(x, y)];
		uint8_t bottom = puz->group_map[CDOK_POS(x, y + 1)];

		if (c != CDOK_GROUP_NONE && c == bottom)
			out |= 1 << x;
	}

	return out;
}

/* Draw a top or bottom border. The joins bitmask tells us which of the
 * vertical gridlines touching this border are minor lines.
 */
static void format_border(const struct cdok_template *templ,
			  const struct cdok_template_hborder *border,
			  int size, int cell_width, cdok_set_t joins,
			  FILE *out)
{
	int x;

	print_char(border->start, 1, out);

	for (x = 0; x < size; x++) {
		print_char(templ->hline_major, cell_width, out);
		if (x + 1 < size)
			print_char((joins & (1 << x)) ?
				   border->tee_minor :
				   border->tee_major, 1, out);
	}

	print_char(border->end, 1, out);
	fprintf(out, "\n");
}

/* Format a horizontal row containing horizontal gridlines and
 * four-way joining characters. Bit X of each field tells us, if it is
 * set:
 *
 *     joins_above: the Xth joiner joins above with a minor vertical
 *                  gridline
 *     joins:       the Xth horizontal gridline is a minor one
 *     joins_below: the Xth joiner joins below with a minor vertical
 *                  gridline
 */
static void format_hline(const struct cdok_template *templ,
			 int size, int cell_width,
			 cdok_set_t joins_above,
			 cdok_set_t joins,
			 cdok_set_t joins_below, FILE *out)
{
	int x;

	print_char((joins & 1) ?
		   templ->tee_left_minor : templ->tee_left_major, 1, out);

	for (x = 0; x < size; x++) {
		print_char((joins & (1 << x)) ?
			   templ->hline_minor : templ->hline_major,
			   cell_width, out);

		if (x + 1 < size) {
			int inner = CDOK_TIN_BTLR;

			if (joins & (1 << x))
				inner &= ~CDOK_TIN_L;
			if (joins & (1 << (x + 1)))
				inner &= ~CDOK_TIN_R;
			if (joins_above & (1 << x))
				inner &= ~CDOK_TIN_T;
			if (joins_below & (1 << x))
				inner &= ~CDOK_TIN_B;

			print_char(templ->inners[inner], 1, out);
		}
	}

	print_char((joins & (1 << (size - 1))) ?
		   templ->tee_right_minor : templ->tee_right_major,
		   1, out);
	fprintf(out, "\n");
}

/* Format a row containing vertical gridlines and cells, optionally
 * containing clues and/or values.
 *
 * The joins field tells us which of the vertical gridlines are minor.
 * Bit X is set if the vertical gridline to the immediate right of cell
 * X is a minor gridline.
 */
static void format_row(const struct cdok_template *templ,
		       const struct cdok_puzzle *puz,
		       int cell_width, int y, cdok_set_t joins,
		       const uint8_t *values, int clues,
		       FILE *out)
{
	int x;

	print_char(templ->vline_major, 1, out);

	for (x = 0; x < puz->size; x++) {
		const cdok_pos_t c = CDOK_POS(x, y);
		char text[16];

		if (values) {
			int len = format_value(text, sizeof(text), values[c]);

			print_char(' ', (cell_width - len) / 2, out);
			fputs(text, out);
			print_char(' ', (cell_width - len + 1) / 2, out);
		} else if (clues) {
			int len = format_clue(text, sizeof(text), puz, c);

			fputs(text, out);
			print_char(' ', cell_width - len, out);
		} else {
			print_char(' ', cell_width, out);
		}

		if (x + 1 < puz->size)
			print_char((joins & (1 << x)) ?
				   templ->vline_minor :
				   templ->vline_major, 1, out);
	}

	print_char(templ->vline_major, 1, out);
	fprintf(out, "\n");
}

/* Render a puzzle using the given template. */
void cdok_format_puzzle(const struct cdok_template *templ,
			const struct cdok_puzzle *puz,
			const uint8_t *values, FILE *out)
{
	int cell_width = 5;
	int x, y;

	/* Calculate a sufficient cell width to hold the longest
	 * clue/value.
	 */
	for (y = 0; y < puz->size; y++)
		for (x = 0; x < puz->size; x++) {
			const cdok_pos_t c = CDOK_POS(x, y);
			char text[16];
			int len = format_clue(text, sizeof(text), puz, c);

			if (len > cell_width)
				cell_width = len;

			len = format_value(text, sizeof(text), values[c]);
			if (len > cell_width)
				cell_width = len;
		}

	/* Top border */
	format_border(templ, &templ->top,
		      puz->size, cell_width, format_hjoins(puz, 0), out);

	for (y = 0; y < puz->size; y++) {
		cdok_set_t h = format_hjoins(puz, y);

		/* Cells occupy three rows. Clues go in the top row, values
		 * in the second, and the third is blank.
		 */
		format_row(templ, puz, cell_width, y, h, NULL, 1, out);
		format_row(templ, puz, cell_width, y, h, values, 0, out);
		format_row(templ, puz, cell_width, y, h, NULL, 0, out);

		/* Horizontal gridlines between cells */
		if (y + 1 < puz->size)
			format_hline(templ, puz->size, cell_width, h,
				     format_vjoins(puz, y),
				     format_hjoins(puz, y + 1), out);
	}

	/* Bottom border */
	format_border(templ, &templ->bottom,
		      puz->size, cell_width,
		      format_hjoins(puz, puz->size - 1), out);
}
