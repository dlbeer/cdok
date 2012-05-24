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

#include "solver.h"

/************************************************************************
 * Group analysis
 *
 * Value sets are bitmasks of possible values. Bit n indicates the presence
 * of the value (n+1) in the set.
 */

/* If N addends in the range [1..max] are used to make the target sum,
 * what is the set of possible addends?
 */
static cdok_set_t addends_for(int target, int n, int max)
{
	int a_min;
	int a_max;

	if (target < 1 || n < 1)
		return 0;

	if (n == 1) {
		if (target >= 1 && target <= max)
			return CDOK_SET_SINGLE(target);

		return 0;
	}

	a_min = target - max * (n - 1);
	if (a_min < 1)
		a_min = 1;

	a_max = target - (n - 1);
	if (a_max > max)
		a_max = max;

	if (a_min > a_max)
		return 0;

	return CDOK_SET_RANGE(a_min, a_max);
}

/* If N factors in the range [1..max] are used to make the target
 * product, what is the set of possible factors?
 */
static cdok_set_t factors_for(int target, int n, int max)
{
	cdok_set_t out = 0;
	int i;

	if (target < 1 || n < 1)
		return 0;

	if (n == 1) {
		if (target >= 1 || target <= max)
			return CDOK_SET_SINGLE(target);

		return 0;
	}

	for (i = 1; i * i < target && i <= max; i++)
		if (!(target % i)) {
			int j = target / i;

			out |= CDOK_SET_SINGLE(i);
			if (j <= max)
				out |= CDOK_SET_SINGLE(j);
		}

	return out;
}

/* Find the set of possible missing values in the partially-filled sum
 * group.
 */
static cdok_set_t sum_candidates(int target, int size,
			     const uint8_t *members, int nm, int max)
{
	int partial_sum = 0;
	int i;

	for (i = 0; i < nm; i++)
		partial_sum += members[i];

	return addends_for(target - partial_sum, size - nm, max);
}

/* Find the set of possible missing values in the partially-filled
 * difference group.
 */
static cdok_set_t difference_candidates(int target, int size,
				    const uint8_t *members, int nm, int max)
{
	cdok_set_t out = 0;
	int partial_sum = 0;
	int max_m = -1;
	int i;

	for (i = 0; i < nm; i++) {
		if (members[i] > max_m)
			max_m = members[i];

		partial_sum += members[i];
	}

	/* It may be that the sum is already present, and we have only
	 * to fill the box with addends.
	 */
	if (nm >= 1)
		out |= addends_for(max_m * 2 - partial_sum - target,
				   size - nm, max);

	/* Or perhaps the sum is missing. Then we need to consider two
	 * sub-cases: where *only* the sum is missing, or a sum and one
	 * or more addends.
	 */
	if (nm + 1 == size) {
		int sum = target + partial_sum;

		if (sum <= max)
			out |= CDOK_SET_SINGLE(sum);
	} else {
		int min_sum = target + partial_sum + (size - nm - 1);
		int i;

		for (i = min_sum; i <= max; i++) {
			cdok_set_t terms_for_sum =
				addends_for(i - partial_sum - target,
					    size - nm - 1, max);

			if (terms_for_sum)
				out |= terms_for_sum | CDOK_SET_SINGLE(i);
		}
	}

	return out;
}

/* Find the set of possible missing values in the partially-filled
 * product group.
 */
static cdok_set_t product_candidates(int target, int size,
				 const uint8_t *members, int nm, int max)
{
	int partial_product = 1;
	int i;

	for (i = 0; i < nm; i++)
		partial_product *= members[i];

	if (target % partial_product)
		return 0;

	return factors_for(target / partial_product, size - nm, max);
}

/* Find the set of possible missing values in the partiall-filled
 * ratio group.
 */
static cdok_set_t ratio_candidates(int target, int size,
			       const uint8_t *members, int nm, int max)
{
	cdok_set_t out = 0;
	int partial_product = 1;
	int max_m = -1;
	int i;

	for (i = 0; i < nm; i++) {
		partial_product *= members[i];

		if (members[i] > max_m)
			max_m = members[i];
	}

	/* Perhaps we're missing only factors, and the product is already
	 * present.
	 */
	if (nm >= 1 && !((max_m * max_m) % (partial_product * target)))
		out |= factors_for((max_m * max_m) / (partial_product * target),
				   size - nm, max);

	/* Perhaps the product is missing. Then there are two sub-cases: only
	 * the product is missing, or the product and one or more factors.
	 */
	if (nm + 1 == size) {
		int product = partial_product * target;

		if (product <= max)
			out |= CDOK_SET_SINGLE(product);
	} else {
		int min_product = partial_product * target;
		int i;

		for (i = 1; i * min_product <= max; i++) {
			cdok_set_t terms_for_product =
				factors_for(i, size - nm - 1, max);

			if (terms_for_product)
				out |= terms_for_product |
					CDOK_SET_SINGLE(i * min_product);
		}
	}

	return out;
}

/* Examine the given group with the current set of grid values and
 * determine the set of values which might fill the remaining empty
 * cells.
 */
static cdok_set_t group_candidates(const struct cdok_group *g,
				   const uint8_t *values, int max)
{
	uint8_t members[CDOK_GROUP_SIZE];
	unsigned int count = 0;
	int i;

	for (i = 0; i < g->size; i++) {
		uint8_t v = values[g->members[i]];

		if (v)
			members[count++] = v;
	}

	switch (g->type) {
	case CDOK_SUM:
		return sum_candidates(g->target, g->size,
				      members, count, max);

	case CDOK_DIFFERENCE:
		return difference_candidates(g->target, g->size,
					     members, count, max);

	case CDOK_PRODUCT:
		return product_candidates(g->target, g->size,
					  members, count, max);

	case CDOK_RATIO:
		return ratio_candidates(g->target, g->size,
					members, count, max);
	}

	return 0;
}

/************************************************************************
 * Row/column analysis
 */

/* Examine the given grid of values and determine, for each cell, which
 * values have not yet been used in the same row/column.
 */
static void build_rc_candidates(const uint8_t *values, cdok_set_t *candidates,
				int max)
{
	cdok_set_t rows[CDOK_SIZE] = {0};
	cdok_set_t cols[CDOK_SIZE] = {0};
	int i;

	/* Collect the sets of values found in each row/column */
	for (i = 0; i < CDOK_CELLS; i++) {
		uint8_t v = values[i];

		if (v) {
			cdok_set_t s = CDOK_SET_SINGLE(v);

			rows[CDOK_POS_Y(i)] |= s;
			cols[CDOK_POS_X(i)] |= s;
		}
	}

	/* Mark each cell with the values not found in that row/column */
	for (i = 0; i < CDOK_CELLS; i++)
		candidates[i] =
			CDOK_SET_ONES(max) ^
			(rows[CDOK_POS_Y(i)] | cols[CDOK_POS_X(i)]);
}

/* Set size */
static int count_bits(cdok_set_t s)
{
	int count = 0;

	while (s) {
		s &= (s - 1);
		count++;
	}

	return count;
}

/* Take a map of candidate values and eliminate candidates for each cell
 * which can't be used to fill the group they belong to.
 */
static void constrain_by_groups(const struct cdok_puzzle *puz,
				const uint8_t *values,
				cdok_set_t *candidates)
{
	int i;

	for (i = 0; i < CDOK_GROUPS; i++) {
		const struct cdok_group *g = &puz->groups[i];

		if (g->size) {
			cdok_set_t c = group_candidates(g, values, puz->size);
			int j;

			for (j = 0; j < g->size; j++)
				candidates[g->members[j]] &= c;
		}
	}
}

/* Find the empty cell in the map of candidate values which has the fewest
 * number of possible values. Returns -1 if there are no empty cells in the
 * grid.
 */
static cdok_pos_t search_least_free(const uint8_t *values,
				    const cdok_set_t *candidates,
				    int max)
{
	cdok_pos_t best = -1;
	int best_count = 0;
	int y;

	for (y = 0; y < max; y++) {
		int x;

		for (x = 0; x < max; x++) {
			const cdok_pos_t c = CDOK_POS(x, y);

			if (!values[c]) {
				int count = count_bits(candidates[c]);

				if (best < 0 || count < best_count) {
					best = c;
					best_count = count;
				}
			}
		}
	}

	return best;
}

/************************************************************************
 * Solver
 */

/* Analyze the puzzle and current set of values. Find an empty cell, if
 * one exists, with the fewest number of possible values that can be
 * filled, and also return the set of candidate values for that cell.
 *
 * Returns -1 if there are no empty cells in the grid.
 */
static cdok_pos_t find_candidates(const struct cdok_puzzle *puz,
				  const uint8_t *values, cdok_set_t *cand_out)
{
	cdok_set_t candidates[CDOK_CELLS];
	cdok_pos_t c;

	build_rc_candidates(values, candidates, puz->size);
	constrain_by_groups(puz, values, candidates);

	/* Choose branches for value-oriented search */
	c = search_least_free(values, candidates, puz->size);
	if (c < 0)
		return -1;

	*cand_out = candidates[c];
	return c;
}

/* Back-tracking solver.
 *
 * At each step, pick the empty cell with the fewest candidate values,
 * try filling in each candidate value and recursively solving. When a
 * solution is found, copy it to the solution grid and increment the
 * count.
 *
 * Search stops when:
 *
 *    (i)  the search tree is exhausted (meaning the puzzle is uniquely
 *         solvable, or unsolvable).
 *    (ii) or, we've found two solutions (meaning the puzzle is solvable,
 *         but not uniquely).
 *
 * While solving, we keep track of a branch difficulty score. This is
 * calculated as the sum of f(B) for each branching factor B on the way
 * from the root of the search tree to the current position:
 *
 *    f(B) = (B-1)^2
 *
 * A solution which requires no backtracking (only a single candidate at
 * each step) would have a branch difficulty of 0.
 */
struct solver_context {
	const struct cdok_puzzle	*puzzle;
	uint8_t				*solution;
	uint8_t				values[CDOK_CELLS];
	unsigned int			count;
	unsigned int			branch_diff;
};

static void solve_recurse(struct solver_context *ctx, int branch_diff)
{
	cdok_pos_t cell;
	cdok_set_t candidates;
	int i;
	int diff;

	cell = find_candidates(ctx->puzzle, ctx->values, &candidates);

	/* Is the puzzle solved? */
	if (cell < 0) {
		if (!ctx->count) {
			if (ctx->solution)
				memcpy(ctx->solution, ctx->values,
				       sizeof(ctx->values));
			ctx->branch_diff = branch_diff;
		}

		ctx->count++;
		return;
	}

	/* Is the puzzle unsolvable? */
	if (!candidates)
		return;

	/* Try backtracking on the most constrained cell/value */
	diff = count_bits(candidates) - 1;
	diff = branch_diff + (diff * diff);

	for (i = 1; i <= ctx->puzzle->size; i++) {
		if (!(candidates & CDOK_SET_SINGLE(i)))
			continue;

		ctx->values[cell] = i;
		solve_recurse(ctx, diff);
		ctx->values[cell] = 0;

		if (ctx->count >= 2)
			return;
	}
}

/* Set up data for the backtracking solver, call it and collect the results.
 *
 * We calculate the final difficulty score as:
 *
 *    D = B * M + E
 *
 * Where
 *
 *    B is the branch difficulty score of the first solution found.
 *    M is a power of 10 greater than the number of cells in the grid.
 *    E is the number of empty cells in the starting arrangement.
 */
int cdok_solve(const struct cdok_puzzle *puz, uint8_t *solution, int *diff)
{
	struct solver_context ctx;

	ctx.puzzle = puz;
	ctx.solution = solution;
	ctx.count = 0;
	memcpy(ctx.values, puz->values, sizeof(ctx.values));

	solve_recurse(&ctx, 0);

	if (!ctx.count)
		return -1;

	if (diff) {
		int m = 1;
		int e = 0;
		int x, y;

		while (m < puz->size * puz->size)
			m *= 10;

		for (y = 0; y < puz->size; y++)
			for (x = 0; x < puz->size; x++)
				if (!puz->values[CDOK_POS(x, y)])
					e++;

		*diff = ctx.branch_diff * m + e;
	}

	return ctx.count > 1 ? 1 : 0;
}
