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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include "cdok.h"
#include "parser.h"
#include "printer.h"
#include "solver.h"

#define OPT_FLAG_UNICODE	0x01

struct command;

struct options {
	int			flags;
	const char		*in_file;
	const char		*out_file;
	const struct command	*command;
};

static int read_puzzle(const char *fname, struct cdok_puzzle *puz)
{
	struct cdok_parser parse;
	FILE *in = stdin;
	char buf[4096];
	int len;

	if (fname) {
		in = fopen(fname, "r");
		if (!in) {
			fprintf(stderr, "Can't open %s for reading: %s\n",
				fname, strerror(errno));
			return -1;
		}
	}

	cdok_parser_init(&parse, puz);
	while ((len = fread(buf, 1, sizeof(buf), in)) > 0)
		if (cdok_parser_push(&parse, puz, buf, len) < 0) {
			if (fname)
				fclose(in);
			return -1;
		}

	if (ferror(in)) {
		fprintf(stderr, "IO error reading %s: %s\n",
			fname, strerror(errno));
		if (fname)
			fclose(in);
		return -1;
	}

	if (fname)
		fclose(in);

	return cdok_parser_end(&parse, puz);
}

static FILE *open_output(const char *fname)
{
	FILE *out = stdout;

	if (fname) {
		out = fopen(fname, "w");
		if (!out) {
			fprintf(stderr, "Can't open %s for writing: %s\n",
				fname, strerror(errno));
			return NULL;
		}
	}

	return out;
}

static int close_output(const char *fname, FILE *out)
{
	if (fname) {
		if (fclose(out) < 0) {
			fprintf(stderr, "Error on closing %s: %s\n",
				fname, strerror(errno));
			return -1;
		}
	}

	return 0;
}

static void write_puzzle(FILE *out, int flags,
			 const struct cdok_puzzle *puz, const uint8_t *values)
{
	cdok_print_puzzle(puz, values, out);
	fprintf(out, "\n");

	cdok_format_puzzle((flags & OPT_FLAG_UNICODE) ?
			   &cdok_template_unicode : &cdok_template_ascii,
			   puz, values, out);
}

static void write_summary(FILE *out, int ret, int diff)
{
	fprintf(out, "Solution is %sunique. Difficulty: %d\n",
		ret ? "not " : "", diff);
}

static int cmd_print(const struct options *opt)
{
	struct cdok_puzzle puz;
	FILE *out;

	if (read_puzzle(opt->in_file, &puz) < 0)
		return -1;

	out = open_output(opt->out_file);
	if (!out)
		return -1;

	write_puzzle(out, opt->flags, &puz, puz.values);
	return close_output(opt->out_file, out);
}

static int do_solve(const struct options *opt, int want_solution)
{
	struct cdok_puzzle puz;
	uint8_t solution[CDOK_CELLS];
	FILE *out;
	int diff = 0;
	int r;

	if (read_puzzle(opt->in_file, &puz) < 0)
		return -1;

	r = cdok_solve(&puz, solution, &diff);
	if (r < 0) {
		fprintf(stderr, "Puzzle is not solvable\n");
		return -1;
	}

	out = open_output(opt->out_file);
	if (!out)
		return -1;

	if (want_solution) {
		write_puzzle(out, opt->flags, &puz, solution);
		fprintf(out, "\n");
	}

	write_summary(out, r, diff);

	return close_output(opt->out_file, out);
}

static int cmd_solve(const struct options *opt)
{
	return do_solve(opt, 1);
}

static int cmd_examine(const struct options *opt)
{
	return do_solve(opt, 0);
}

struct command {
	const char	*name;
	int		(*func)(const struct options *opt);
};

static const struct command command_table[] = {
	{"print",		cmd_print},
	{"solve",		cmd_solve},
	{"examine",		cmd_examine},
	{NULL, NULL}
};

static const struct command *find_command(const char *name)
{
	int i = 0;

	for (;;) {
		const struct command *c = &command_table[i];

		if (!c->name)
			break;

		if (!strcasecmp(c->name, name))
			return c;

		i++;
	}

	return NULL;
}

static void usage(const char *progname)
{
	printf("Usage: %s [options] <command>\n"
"\n"
"Options are:\n"
"    -u           Use Unicode (UTF-8) line-drawing characters.\n"
"    -i filename  Read from the given file (default stdin).\n"
"    -o filename  Write to the given file (default stdout).\n"
"    --help       Show this text.\n"
"    --version    Show version information.\n"
"\n"
"Available commands:\n"
"    print        Parse a grid spec and print it.\n"
"    solve        Parse a grid spec and solve the puzzle.\n"
"    examine      Parse a grid spec and estimate difficulty.\n",
	       progname);
}

static void version(void)
{
	printf("cdok -- Calcudoku solver/generator\n"
"Copyright (C) 2012 Daniel Beer <dlbeer@gmail.com>\n"
"\n"
"Permission to use, copy, modify, and/or distribute this software for any\n"
"purpose with or without fee is hereby granted, provided that the above\n"
"copyright notice and this permission notice appear in all copies.\n"
"\n"
"THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL WARRANTIES\n"
"WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF\n"
"MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR\n"
"ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES\n"
"WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN\n"
"ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF\n"
"OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.\n");
}

static int parse_options(int argc, char **argv, struct options *opt)
{
	static const struct option longopts[] = {
		{"help",	0, 0, 'H'},
		{"version",	0, 0, 'V'},
		{NULL, 0, 0, 0}
	};
	int o;

	memset(opt, 0, sizeof(*opt));

	while ((o = getopt_long(argc, argv, "i:o:u", longopts, NULL)) >= 0)
		switch (o) {
		case 'u':
			opt->flags |= OPT_FLAG_UNICODE;
			break;

		case 'i':
			opt->in_file = optarg;
			break;

		case 'o':
			opt->out_file = optarg;
			break;

		case 'V':
			version();
			exit(0);

		case 'H':
			usage(argv[0]);
			exit(0);
		}

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		fprintf(stderr, "You need to specify a command. Try --help.\n");
		return -1;
	}

	opt->command = find_command(argv[0]);
	if (!opt->command) {
		fprintf(stderr, "Unknown command: %s. Try --help.\n", argv[0]);
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct options opt;

	if (parse_options(argc, argv, &opt) < 0)
		return -1;

	return opt.command->func(&opt);
}
