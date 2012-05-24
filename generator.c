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

#include <stdlib.h>
#include <string.h>

#include "cdok.h"
#include "solver.h"
#include "generator.h"

/************************************************************************
 * Puzzle generator: grid generation
 */

/* Generate a random permutation of the numbers [1..size] */
static void gen_permutation(int size, uint8_t *values)
{
	int i;

	for (i = 0; i < size; i++)
		values[i] = i + 1;

	for (i = size - 1; i >= 1; i--) {
		int src = random() % (i + 1);
		int tmp = values[i];

		values[i] = values[src];
		values[src] = tmp;
	}
}

/* Recursive grid fill. The grid is filled by picking the next empty
 * cell in the sequence, and trying each of the possible values in random
 * order. If we reach a cell for which there are no possible values, we
 * need to backtrack and try again.
 */
struct fill_context {
	int		size;
	uint8_t		*grid;
	cdok_set_t	rows_used[CDOK_SIZE];
	cdok_set_t	cols_used[CDOK_SIZE];
};

static int fill_grid(struct fill_context *ctx, int x, int y)
{
	cdok_set_t used;
	uint8_t choices[CDOK_SIZE];
	int i;
	cdok_pos_t c;

	if (x >= ctx->size) {
		x = 0;
		y++;
	}

	if (y >= ctx->size)
		return 0;

	gen_permutation(ctx->size, choices);

	used = ctx->rows_used[y] | ctx->cols_used[x];
	c = CDOK_POS(x, y);

	for (i = 0; i < ctx->size; i++) {
		const int v = choices[i];
		const cdok_set_t mask = CDOK_SET_SINGLE(v);

		if (used & mask)
			continue;

		ctx->rows_used[y] |= mask;
		ctx->cols_used[x] |= mask;
		ctx->grid[c] = v;

		if (!fill_grid(ctx, x + 1, y))
			return 0;

		ctx->rows_used[y] &= ~mask;
		ctx->cols_used[x] &= ~mask;
	}

	return -1;
}

/* Generate a random valid solution grid. */
void cdok_generate_grid(uint8_t *values, int size)
{
	struct fill_context ctx;
	uint8_t top_row[CDOK_SIZE];
	int i;

	memset(values, 0, CDOK_CELLS * sizeof(values[0]));

	memset(&ctx, 0, sizeof(ctx));
	ctx.size = size;
	ctx.grid = values;

	gen_permutation(size, top_row);
	for (i = 0; i < size; i++) {
		values[CDOK_POS(i, 0)] = top_row[i];
		ctx.cols_used[i] = CDOK_SET_SINGLE(top_row[i]);
	}

	if (fill_grid(&ctx, 0, 1) < 0)
		abort();
}

/************************************************************************
 * Puzzle generator: basic group operations (invariant-breaking)
 *
 * These operations preserve the group_map, but may violate geometry
 * constraints.
 */

/* Find an unused group and return the index. */
static int group_alloc(struct cdok_puzzle *puz)
{
	int i;

	for (i = 0; i < CDOK_GROUPS; i++)
		if (!puz->groups[i].size)
			return i;

	return CDOK_GROUP_NONE;
}

/* Destroy a group and turn all its members back into value cells. */
static void group_destroy(struct cdok_puzzle *puz, int grp,
			  const uint8_t *solution)
{
	struct cdok_group *g = &puz->groups[grp];
	int i;

	for (i = 0; i < g->size; i++) {
		const cdok_pos_t c = g->members[i];

		puz->values[c] = solution[c];
		puz->group_map[c] = CDOK_GROUP_NONE;
	}

	g->size = 0;
}

/* Remove the given cell from the group. */
static void group_remove(struct cdok_puzzle *puz, int grp,
			 cdok_pos_t victim,
			 const uint8_t *solution)
{
	struct cdok_group *g = &puz->groups[grp];
	int i;

	for (i = 0; i < g->size; i++) {
		const cdok_pos_t c = g->members[i];

		if (c == victim) {
			g->members[i] = g->members[g->size - 1];
			g->size--;
			puz->values[c] = solution[c];
			puz->group_map[c] = CDOK_GROUP_NONE;
			break;
		}
	}
}

/* Add the given cell to the group if it doesn't already belong to one.
 * No geometry checks are performed.
 */
static void group_add(struct cdok_puzzle *puz, int grp, cdok_pos_t c)
{
	struct cdok_group *g = &puz->groups[grp];

	if (puz->group_map[c] != CDOK_GROUP_NONE)
		return;

	if (g->size >= CDOK_GROUP_SIZE)
		return;

	g->members[g->size++] = c;
	puz->values[c] = 0;
	puz->group_map[c] = grp;
}

/* Update the target value for a group, if it can possibly be
 * constructed. Returns 0 for success, -1 if no valid target exists for
 * this combination of values and group type.
 */
static int group_update_target(struct cdok_puzzle *puz, int grp,
			       const uint8_t *solution,
			       cdok_flags_t f)
{
	struct cdok_group *g = &puz->groups[grp];
	int sum = 0;
	int product = 1;
	int max = -1;
	int i;

	if (!g->size)
		return 0;

	if ((g->type == CDOK_DIFFERENCE || g->type == CDOK_RATIO) &&
	    g->size > 2 && (f & CDOK_FLAGS_TWO_CELL))
		return -1;

	for (i = 0; i < g->size; i++) {
		const uint8_t v = solution[g->members[i]];

		if (v > max)
			max = v;

		sum += v;
		product *= v;
	}

	switch (g->type) {
	case CDOK_SUM:
		g->target = sum;
		break;

	case CDOK_DIFFERENCE:
		if (max < 0 || max * 2 < sum)
			return -1;
		g->target = max * 2 - sum;
		break;

	case CDOK_PRODUCT:
		g->target = product;
		break;

	case CDOK_RATIO:
		if ((max * max) % product)
			return -1;
		g->target = max * max / product;
		break;
	}

	return 0;
}

/************************************************************************
 * Puzzle generator: mutations (invariant-preserving)
 */

/* Choose a cell at random. */
static cdok_pos_t choose_cell(int size)
{
	return CDOK_POS(random() % size, random() % size);
}

/* Randomly choose one of the given cell's four neighbours. */
static cdok_pos_t choose_neighbour(int size, cdok_pos_t c)
{
	int x = CDOK_POS_X(c);
	int y = CDOK_POS_Y(c);
	int xn = x + 1, yn = y + 1;

	if (xn >= size || (x && (random() & 1)))
		xn = x - 1;
	if (yn >= size || (y && (random() & 1)))
		yn = y - 1;

	if (random() & 1)
		return CDOK_POS(xn, y);

	return CDOK_POS(x, yn);
}

/* Correct the geometry of a group and cut off any non-contigous
 * regions.
 */
static void cut_islands(struct cdok_puzzle *puz, int grp,
			const uint8_t *solution)
{
	uint8_t map_copy[CDOK_CELLS];
	struct cdok_group *g = &puz->groups[grp];
	cdok_pos_t start = g->members[0];
	int len = 0;
	int i;

	if (!g->size)
		return;

	memcpy(map_copy, puz->group_map, sizeof(map_copy));
	cdok_flood_fill(map_copy, grp, CDOK_POS_X(start), CDOK_POS_Y(start));

	for (i = 0; i < g->size; i++) {
		cdok_pos_t c = g->members[i];

		if (map_copy[c] != CDOK_GROUP_NONE) {
			puz->values[c] = solution[c];
			puz->group_map[c] = CDOK_GROUP_NONE;
		} else {
			g->members[len++] = c;
		}
	}

	g->size = len;

	if (g->size < 2)
		group_destroy(puz, grp, solution);
}

/* Randomly alter the type of the given group. Guaranteed to set a type
 * which is valid for the group's values.
 */
static void mut_alter_type(struct cdok_puzzle *puz, int grp,
			   const uint8_t *solution, cdok_flags_t f)
{
	cdok_gtype_t types[] = {
		CDOK_SUM,
		CDOK_DIFFERENCE,
		CDOK_PRODUCT,
		CDOK_RATIO
	};
	int i;

	for (i = 3; i >= 1; i--) {
		int j = random() % (i + 1);
		int tmp = types[i];

		types[i] = types[j];
		types[j] = tmp;
	}

	for (i = 0; i < 4; i++) {
		puz->groups[grp].type = types[i];
		if (!group_update_target(puz, grp, solution, f))
			break;
	}
}

/* Remove the given cell from its group. If necessary, the group will
 * be destroyed and/or pruned to maintain geometry constraints. The
 * group type may also be altered if necessary.
 */
static void mut_remove_cell(struct cdok_puzzle *puz, cdok_pos_t c,
			    const uint8_t *solution, cdok_flags_t f)
{
	int grp = puz->group_map[c];

	if (grp == CDOK_GROUP_NONE)
		return;

	if (puz->groups[grp].size <= 2) {
		group_destroy(puz, grp, solution);
		return;
	}

	group_remove(puz, grp, c, solution);
	cut_islands(puz, grp, solution);

	if (group_update_target(puz, grp, solution, f) < 0)
		mut_alter_type(puz, grp, solution, f);
}

/* Join the given cell (c) so that it belongs to the same group as its
 * neighbour. A group will be created if (n) doesn't belong to one. If
 * (c) already belongs to a group, it will be removed from that group.
 *
 * Adjustments are made to both groups if necessary to preserve geometry
 * and type/target constraints.
 */
static void mut_join_cells(struct cdok_puzzle *puz,
			   cdok_pos_t c, cdok_pos_t n,
			   const uint8_t *solution,
			   cdok_flags_t f)
{
	int ngrp = puz->group_map[n];
	int cgrp = puz->group_map[c];

	if (cgrp != CDOK_GROUP_NONE) {
		if (ngrp == cgrp)
			return;

		mut_remove_cell(puz, c, solution, f);
	}

	if (ngrp != CDOK_GROUP_NONE) {
		group_add(puz, ngrp, c);
		if (group_update_target(puz, ngrp, solution, f) < 0)
			mut_alter_type(puz, ngrp, solution, f);
	} else {
		int g = group_alloc(puz);

		if (g == CDOK_GROUP_NONE)
			return;

		group_add(puz, g, c);
		group_add(puz, g, n);
		mut_alter_type(puz, g, solution, f);
	}
}

/************************************************************************
 * Puzzle generator
 */

/* Ensure that the first cell in each group is the one with the smallest
 * grid index.
 */
static void normalize_labels(struct cdok_puzzle *puz)
{
	int i;

	for (i = 0; i < CDOK_GROUPS; i++) {
		struct cdok_group *g = &puz->groups[i];
		int min = 0;
		int j;

		if (!g->size)
			continue;

		for (j = 1; j < g->size; j++)
			if (g->members[j] < g->members[min])
				min = j;

		j = g->members[min];
		g->members[min] = g->members[0];
		g->members[0] = j;
	}
}

/* Perform a hardening iteration on the given puzzle. We make 10 random
 * invariant-preserving changes to the puzzle in sequence. After each
 * change, we check to see if there's a unique solution. If so, and the
 * puzzle has become more difficult, save it.
 */
static int harden(struct cdok_puzzle *puz, const uint8_t *solution,
		  int best_score_in, cdok_flags_t fl, int limit)
{
	int best_score = best_score_in;
	struct cdok_puzzle work;
	int i;

	memcpy(&work, puz, sizeof(work));

	for (i = 0; i < 10; i++) {
		cdok_pos_t c = choose_cell(puz->size);
		cdok_pos_t cn = choose_neighbour(puz->size, c);
		int score = 0;
		int r;

		mut_join_cells(&work, c, cn, solution, fl);
		r = cdok_solve(&work, NULL, &score);

		if (!r && (score > best_score) &&
		    (limit <= 0 || score <= limit)) {
			memcpy(puz, &work, sizeof(*puz));
			best_score = score;
		}
	}

	return best_score;
}

/* Create a puzzle with the given solution and harden it until we reach
 * the maximum iteration count or the difficulty threshold.
 */
int cdok_generate(struct cdok_puzzle *puz,
		  const uint8_t *solution, int size,
		  cdok_flags_t fl, int iterations, int limit, int target)
{
	int best_score = 0;
	int i;

	if (size < 2)
		return 0;

	cdok_init_puzzle(puz, size);
	memcpy(puz->values, solution, sizeof(puz->values));

	for (i = 0; i < iterations; i++) {
		if (target > 0 && best_score >= target)
			break;

		best_score = harden(puz, solution, best_score, fl,
				    limit);
	}

	normalize_labels(puz);
	return best_score;
}
