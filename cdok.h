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

#ifndef CDOK_H_
#define CDOK_H_

/* This header file defines basic data structures and utilities for
 * representing puzzles and grids. A grid is represented using a flat
 * array of size CDOK_CELLS. Coordinates are converted to and from
 * array indices (cdok_pos_t) via the CDOK_POS* macros.
 */

#include <stdint.h>

/* Limits on puzzle and group sizes. */
#define	CDOK_SIZE		16
#define CDOK_GROUPS		52
#define CDOK_CELLS		256
#define CDOK_GROUP_SIZE		8

/* Position reference. This is an index into a grid. */
typedef int cdok_pos_t;

#define CDOK_POS(x, y)	(((y) << 4) | (x))
#define CDOK_POS_X(c)	((c) & 0xf)
#define CDOK_POS_Y(c)	((c) >> 4)

/* Group types. */
typedef enum {
	CDOK_SUM = '+',
	CDOK_DIFFERENCE = '-',
	CDOK_PRODUCT = '*',
	CDOK_RATIO = '/'
} cdok_gtype_t;

/* Clue group. This consists of a type, clue value (target), and a list
 * of member cells, specified as position references.
 *
 * A size of 0 indicates that the group is unused/unallocated.
 */
struct cdok_group {
	cdok_gtype_t	type;
	int		target;
	unsigned int	size;
	cdok_pos_t	members[CDOK_GROUP_SIZE];
};

#define CDOK_GROUP_NONE		0xff

/* Puzzle representation. This consists of:
 *
 *   size:      the dimensions of the puzzle (<= CDOK_SIZE)
 *   values:    given clues, indexed by cdok_pos_t. A value of zero
 *              indicates that there is no given value for this cell.
 *   groups:    a list of grouped cells and their clues.
 *   group_map: a mapping from cells to the groups to which they belong,
 *              if any (indexed by cdok_pos_t). A value of CDOK_GROUP_NONE
 *              indicates that the cell does not belong to a group.
 *
 * The following basic invariants should hold:
 *
 *    * 1 <= size <= CDOK_SIZE
 *    * The size of any valid group is >= 2.
 *    * If group_map[c] == g for every cell c in group g.
 *    * Cells are a member of one group only, at most.
 *    * Groups are composed of contiguous cells -- you can trace a route
 *      from any cell in a group to any other, travelling only
 *      up/down/left/right.
 */
struct cdok_puzzle {
	unsigned int		size;
	struct cdok_group	groups[CDOK_GROUPS];
	uint8_t			values[CDOK_CELLS];
	uint8_t			group_map[CDOK_CELLS];
};

/* This data type is used to represent sets of values. It's guaranteed
 * to have at least CDOK_SIZE bits.
 */
typedef uint16_t cdok_set_t;

#define CDOK_SET_SINGLE(c)		(1 << ((c) - 1))
#define CDOK_SET_ONES(s)		((1 << (s)) - 1)
#define CDOK_SET_RANGE(min, max)	\
	(CDOK_SET_ONES(max - min + 1) << (min - 1))

/* Initialize a new, empty grid */
void cdok_init_puzzle(struct cdok_puzzle *puz, int size);

/* Flood-fill a group map, setting cells from the source value to
 * CDOK_GROUP_NONE. This function is used to check for valid geometry.
 */
void cdok_flood_fill(uint8_t *map, uint8_t src, int x, int y);

/* Each group is named in a puzzle spec using an alphabetic character.
 * These functions map group indices to and from characters.
 */
uint8_t cdok_group_to_char(uint8_t g);
uint8_t cdok_char_to_group(uint8_t ch);

#endif
