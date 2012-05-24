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

#include <string.h>

#include "cdok.h"

/* Map a character to a group ID */
uint8_t cdok_char_to_group(uint8_t ch)
{
	if (ch >= 'A' && ch <= 'Z')
		return ch - 'A';

	if (ch >= 'a' && ch <= 'z')
		return ch - 'a' + 26;

	return CDOK_GROUP_NONE;
}

/* Map a group ID to a character */
uint8_t cdok_group_to_char(uint8_t g)
{
	if (g < 26)
		return g + 'A';

	return g - 26 + 'a';
}

void cdok_init_puzzle(struct cdok_puzzle *puz, int size)
{
	int i;

	memset(puz, 0, sizeof(*puz));

	for (i = 0; i < CDOK_GROUPS; i++)
		puz->groups[i].target = -1;

	puz->size = size;
	memset(puz->group_map, CDOK_GROUP_NONE, sizeof(puz->group_map));
}

/* Flood fill from the src value to 0 on the given map. */
void cdok_flood_fill(uint8_t *map, uint8_t src, int x, int y)
{
	int start_x = x;
	int end_x = x;
	int i;

	if (map[CDOK_POS(x, y)] != src)
		return;

	while (start_x > 0 && map[CDOK_POS(start_x - 1, y)] == src)
		start_x--;

	while (end_x + 1 < CDOK_SIZE && map[CDOK_POS(end_x + 1, y)] == src)
		end_x++;

	for (i = start_x; i <= end_x; i++)
		map[CDOK_POS(i, y)] = CDOK_GROUP_NONE;

	if (y > 0)
		for (i = start_x; i <= end_x; i++)
			cdok_flood_fill(map, src, i, y - 1);

	if (y + 1 < CDOK_SIZE)
		for (i = start_x; i <= end_x; i++)
			cdok_flood_fill(map, src, i, y + 1);
}
