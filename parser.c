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

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "cdok.h"
#include "parser.h"

/* Check that all groups are single contiguous regions. On a copy of the
 * group map, flood-fill from a single member of each group. If the groups
 * are all contiguous, this should completely erase the map.
 */
static int validate_group_map(const struct cdok_puzzle *puz)
{
	uint8_t map[CDOK_CELLS];
	int i;

	memcpy(map, puz->group_map, sizeof(map));

	for (i = 0; i < CDOK_GROUPS; i++) {
		const struct cdok_group *g = &puz->groups[i];
		cdok_pos_t start = g->members[0];

		if (g->size)
			cdok_flood_fill(map, i,
					CDOK_POS_X(start), CDOK_POS_Y(start));
	}

	for (i = 0; i < CDOK_CELLS; i++)
		if (map[i] != CDOK_GROUP_NONE) {
			fprintf(stderr, "Group %c is not contiguous at "
				"cell (%d, %d)\n",
				cdok_group_to_char(map[i]),
				CDOK_POS_X(i), CDOK_POS_Y(i));
			return -1;
		}

	return 0;
}

/* Check that each group has a target and type and satisfies basic
 * constraints.
 */
static int validate_groups(const struct cdok_puzzle *puz)
{
	int i;

	for (i = 0; i < CDOK_GROUPS; i++) {
		const struct cdok_group *g = &puz->groups[i];
		uint8_t ch = cdok_group_to_char(i);

		if (!g->size)
			continue;

		if (!g->type) {
			fprintf(stderr, "Group %c has no type\n", ch);
			return -1;
		}

		if (g->target < 0) {
			fprintf(stderr, "Group %c has no target\n", ch);
			return -1;
		}

		if (g->size < 2) {
			fprintf(stderr, "Group %c has only a "
				"single member\n", ch);
			return -1;
		}

		if ((g->type == CDOK_RATIO || g->type == CDOK_PRODUCT) &&
		    !g->target) {
			fprintf(stderr, "Group %c is of type %c but it has "
				"a target of 0\n", ch, g->type);
			return -1;
		}
	}

	return 0;
}

/* Initialize a parser and clear the puzzle state. */
void cdok_parser_init(struct cdok_parser *p, struct cdok_puzzle *puz)
{
	memset(p, 0, sizeof(*p));
	p->group_name = CDOK_GROUP_NONE;
	p->value = -1;
	cdok_init_puzzle(puz, 0);
}

/* This is an internal parser function called whenever whitespace is
 * encountered to end a cell.
 */
static int parser_end_cell(struct cdok_parser *p, struct cdok_puzzle *puz)
{
	if (p->value < 0 && p->group_name == CDOK_GROUP_NONE)
		return 0;

	if (p->x >= CDOK_SIZE || p->y >= CDOK_SIZE) {
		fprintf(stderr, "Maximum cell coordinates exceeded: (%d, %d)\n",
			p->x, p->y);
		return -1;
	}

	if (p->group_name != CDOK_GROUP_NONE) {
		struct cdok_group *g = &puz->groups[p->group_name];

		if (g->size >= CDOK_GROUP_SIZE) {
			fprintf(stderr, "Maximum group size exceeded: (%d, %d) "
				"(group %c)\n", p->x, p->y, p->group_name);
			return -1;
		}

		g->members[g->size++] = CDOK_POS(p->x, p->y);

		if (p->value >= 0) {
			if (g->target >= 0 && p->value != g->target) {
				fprintf(stderr, "Group %c has two conflicting "
					"targets: %d vs %d\n",
					p->group_name, p->value, g->target);
				return -1;
			}

			g->target = p->value;
		}

		if (p->group_type) {
			if (g->type && g->type != p->group_type) {
				fprintf(stderr, "Group %c has two conflicting "
					"types: %c vs %c\n",
					p->group_name, g->type, p->group_type);
				return -1;
			}

			g->type = p->group_type;
		}
	} else {
		puz->values[CDOK_POS(p->x, p->y)] = p->value;
	}

	p->group_type = 0;
	p->group_name = CDOK_GROUP_NONE;
	p->value = -1;
	p->x++;

	return 0;
}

int cdok_parser_push(struct cdok_parser *p, struct cdok_puzzle *puz,
		     const char *text, int len)
{
	if (p->eof)
		return 0;

	while (len) {
		if (*text == '\n') {
			if (parser_end_cell(p, puz) < 0)
				return -1;

			if (!p->x) {
				p->eof = 1;
				return 0;
			}

			if (!p->y) {
				puz->size = p->x;
			} else if (p->x != puz->size) {
				fprintf(stderr, "Jagged row %d (expected "
					"%d cells)\n", p->y, puz->size);
				return -1;
			}

			p->y++;
			p->x = 0;
		} else if (isspace(*text)) {
			if (parser_end_cell(p, puz) < 0)
				return -1;
		} else if (isdigit(*text)) {
			if (p->value < 0)
				p->value = 0;

			p->value = p->value * 10 + *text - '0';
		} else if (*text == '+' || *text == '-' ||
			   *text == '*' || *text == '/') {
			p->group_type = *text;
		} else {
			uint8_t g = cdok_char_to_group(*text);

			if (g != CDOK_GROUP_NONE)
				p->group_name = g;
		}

		text++;
		len--;
	}

	return 0;
}

/* Rebuild the map of cells to groups. */
static void build_group_map(struct cdok_puzzle *puz)
{
	int i;

	memset(puz->group_map, CDOK_GROUP_NONE, sizeof(puz->group_map));

	for (i = 0; i < CDOK_GROUPS; i++) {
		const struct cdok_group *g = &puz->groups[i];
		unsigned int j;

		for (j = 0; j < g->size; j++)
			puz->group_map[g->members[j]] = i;
	}
}

int cdok_parser_end(struct cdok_parser *p, struct cdok_puzzle *puz)
{
	if (parser_end_cell(p, puz) < 0)
		return -1;

	if (!puz->size) {
		fprintf(stderr, "No cells!\n");
		return -1;
	}

	if (p->y < puz->size) {
		fprintf(stderr, "Grid is not square (width = %d, "
			"height = %d)\n", puz->size, p->y);
		return -1;
	}

	if (validate_groups(puz) < 0)
		return -1;

	build_group_map(puz);

	return validate_group_map(puz);
}
