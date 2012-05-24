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

#ifndef PARSER_H_
#define PARSER_H_

#include "cdok.h"

/* Parser state. */
struct cdok_parser {
	unsigned int	eof;
	unsigned int	x;
	unsigned int	y;
	unsigned int	max_x;
	int		value;
	unsigned int	group_name;
	cdok_gtype_t	group_type;
};

/* Create a new parser and clear the given puzzle grid. */
void cdok_parser_init(struct cdok_parser *p, struct cdok_puzzle *puz);

/* Feed text to the parser. Returns 0 on success or -1 if an error
 * occurs. After the last of the text has been given to the parser, you
 * must call cdok_parser_end().
 */
int cdok_parser_push(struct cdok_parser *p, struct cdok_puzzle *puz,
		     const char *text, int len);

/* Finish parsing and validate the puzzle. Returns 0 on success or -1
 * if an error occurs. Validation ensures that the puzzle satisfies all
 * invariants (except solvability).
 */
int cdok_parser_end(struct cdok_parser *p, struct cdok_puzzle *puz);

#endif
