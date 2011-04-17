/*
 * Copyright (c) 2011 Stefan Rinkes <stefan.rinkes@gmail.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
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

#include <ctype.h>
#include "pfcommon.h"
#include "pfutils.h"

static int	pfrules_init(void);
static int	pfrules_read(void);
#ifndef TEST
static void	submit_counter(const char *, const char *, counter_t, int);
#endif

int
pfrules_init(void)
{
	struct pf_status	status;

	if ((dev = open(PF_SOCKET, O_RDWR)) == -1) {
		return (-1);
	}
	if (ioctl(dev, DIOCGETSTATUS, &status) == -1) {
		return (-1);
	}

	close(dev);
	if (!status.running)
		return (-1);

	return (0);
}

#ifndef TEST
void
submit_counter(const char *rule_number, const char *inst, counter_t val, int usegauge)
{
	value_t		values[1];
	value_list_t	vl = VALUE_LIST_INIT;

	if (usegauge)
		values[0].gauge = val;
	else
		values[0].counter = val;

	vl.values = values;
	vl.values_len = 1;
	sstrncpy (vl.host, hostname_g, sizeof (vl.host));
	sstrncpy (vl.plugin, "pfrules", sizeof (vl.plugin));
	sstrncpy (vl.type, rule_number, sizeof(vl.type));
	sstrncpy (vl.type_instance, inst, sizeof(vl.type_instance));
	plugin_dispatch_values(&vl);
}
#endif


int
pfrules_read(void)
{
	struct pfioc_rule	 pr;
	struct pf_rule		 rule;
	u_int32_t		 nr, mnr;
	char			*path;
	char			 sfn[23];
	FILE			 *sfp = NULL;
	int			 fd;
	char			 rulestring[256];
	static int		nattype[3] = { PF_NAT, PF_RDR, PF_BINAT };
	int			i;
	char			anchorname[MAXPATHLEN];

	memset(anchorname, 0, sizeof(anchorname));

	if ((path = calloc(1, MAXPATHLEN)) == NULL)
		return (-1);

	memset(&pr, 0, sizeof(pr));
	memcpy(pr.anchor, path, sizeof(pr.anchor));

	if ((dev = open(PF_SOCKET, O_RDWR)) == -1) {
		return (-1);
	}

	pr.rule.action = PF_SCRUB;
	if (ioctl(dev, DIOCGETRULES, &pr)) {
		warn("DIOCGETRULES");
		return (-1);
	}

	mnr = pr.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pr.nr = nr;
		if (ioctl(dev, DIOCGETRULE, &pr)) {
			warn("DIOCGETRULE");
			return (-1);
		}
		rule = pr.rule;
		strlcpy(sfn, "/tmp/pfutils.XXXXXXXXXX", sizeof(sfn));
		if ((fd = mkstemp(sfn)) == -1 ||
		    (sfp = fdopen(fd, "w+")) == NULL) {
			if (fd != -1) {
				unlink(sfn);
				close(fd);
			}
		}
		freopen(sfn, "w", stdout);
		print_rule(&pr.rule, pr.anchor_call, 0);
		freopen("/dev/tty", "w", stdout);
		fgets(rulestring, sizeof(rulestring), sfp);
		fclose(sfp);
		remove(sfn);

		char *p1 = rulestring;
		char *p2 = rulestring;
		while(*p1 != 0) {
			if(*p1 == '!') {
				*p2++ = *p1++;
				++p1;
			} else if(*p1 == '=') {
				*--p2 = "";
				*p2++ = *p1++;
				++p1;
			} else if(isspace(*p1)) {
				*p2++ = '-';
				++p1;
			} else {
				*p2++ = *p1++;
			}
		}
		*p2 = 0;

#ifndef TEST
		submit_counter("states_cur", rulestring,
		    rule.states_cur, 1);
		submit_counter("states_tot", rulestring,
		    rule.states_tot, 0);
		submit_counter("evaluations", rulestring,
		    (unsigned long long)rule.evaluations, 0);
		submit_counter("pf_bytes", rulestring,
		    (unsigned long long)(rule.packets[0] + rule.packets[1]), 0);
#else
		printf("Rule-Number: %i\n", rule.nr);
		printf("Rule: %s\n", rulestring);
		printf("States cur: %-6u\n", rule.states_cur);
		printf("States tot: %-6u\n", rule.states_tot);
		printf("Evaluations: %-8llu\n",
		    (unsigned long long)rule.evaluations);
		printf("Bytes: %-10llu\n",
		    (unsigned long long)(rule.packets[0] + rule.packets[1]));
		printf("\n");
#endif
	}

	pr.rule.action = PF_PASS;
	if (ioctl(dev, DIOCGETRULES, &pr)) {
		warn("DIOCGETRULES");
		return (-1);
	}

	mnr = pr.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pr.nr = nr;
		if (ioctl(dev, DIOCGETRULE, &pr)) {
			warn("DIOCGETRULE");
			return (-1);
		}
		rule = pr.rule;

		strlcpy(sfn, "/tmp/pfutils.XXXXXXXXXX", sizeof(sfn));
		if ((fd = mkstemp(sfn)) == -1 ||
		    (sfp = fdopen(fd, "w+")) == NULL) {
			if (fd != -1) {
				unlink(sfn);
				close(fd);
			}
		}

		freopen(sfn, "w", stdout);
		print_rule(&pr.rule, pr.anchor_call, 0);
		freopen("/dev/tty", "w", stdout);
		fgets(rulestring, sizeof(rulestring), sfp);
		fclose(sfp);
		remove(sfn);

		char *p1 = rulestring;
		char *p2 = rulestring;
		while(*p1 != 0) {
			if(*p1 == '!') {
				*p2++ = *p1++;
				++p1;
			} else if(*p1 == '=') {
				*--p2 = "";
				*p2++ = *p1++;
				++p1;
			} else if(isspace(*p1)) {
				*p2++ = '-';
				++p1;
			} else {
				*p2++ = *p1++;
			}
		}
		*p2 = 0;

#ifndef TEST
		submit_counter("states_cur", rulestring,
		    rule.states_cur, 1);
		submit_counter("states_tot", rulestring,
		    rule.states_tot, 0);
		submit_counter("evaluations", rulestring,
		    (unsigned long long)rule.evaluations, 0);
		submit_counter("pf_bytes", rulestring,
		    (unsigned long long)(rule.packets[0] + rule.packets[1]), 0);
#else
		printf("Rule-Number: %i\n", rule.nr);
		printf("Rule: %s\n", rulestring);
		printf("States cur: %-6u\n", rule.states_cur);
		printf("States tot: %-6u\n", rule.states_tot);
		printf("Evaluations: %-8llu\n",
		    (unsigned long long)rule.evaluations);
		printf("Bytes: %-10llu\n",
		    (unsigned long long)(rule.packets[0] + rule.packets[1]));
		printf("\n");
#endif
	}

	for (i = 0; i < 3; i++) {
		pr.rule.action = nattype[i];
		if (ioctl(dev, DIOCGETRULES, &pr)) {
			warn("DIOCGETRULES");
			return (-1);
		}
		mnr = pr.nr;
		for (nr = 0; nr < mnr; ++nr) {
			pr.nr = nr;
			if (ioctl(dev, DIOCGETRULE, &pr)) {
				warn("DIOCGETRULE");
				return (-1);
			}
			if (pfctl_get_pool(dev, &pr.rule.rpool, nr,
			    pr.ticket, nattype[i], anchorname) != 0)
				return (-1);

			rule = pr.rule;

			strlcpy(sfn, "/tmp/pfutils.XXXXXXXXXX", sizeof(sfn));
			if ((fd = mkstemp(sfn)) == -1 ||
			    (sfp = fdopen(fd, "w+")) == NULL) {
				if (fd != -1) {
					unlink(sfn);
					close(fd);
				}
			}

			freopen(sfn, "w", stdout);
			print_rule(&pr.rule, pr.anchor_call, 0);
			freopen("/dev/tty", "w", stdout);
			fgets(rulestring, sizeof(rulestring), sfp);
			fclose(sfp);
			remove(sfn);

			char *p1 = rulestring;
			char *p2 = rulestring;
			while(*p1 != 0) {
				if(*p1 == '!') {
					*p2++ = *p1++;
					++p1;
				} else if(*p1 == '=') {
					*--p2 = "";
					*p2++ = *p1++;
					++p1;
				} else if(isspace(*p1)) {
					*p2++ = '-';
					++p1;
				} else if((*p1 == '(') || (*p1 == ')') || (*p1 == '>')) {
					++p1;
				} else {
					*p2++ = *p1++;
				}
			}
			*p2 = 0;

			pfctl_clear_pool(&pr.rule.rpool);

#ifndef TEST
			submit_counter("states_cur", rulestring,
			    rule.states_cur, 1);
			submit_counter("states_tot", rulestring,
			    rule.states_tot, 0);
			submit_counter("evaluations", rulestring,
			    (unsigned long long)rule.evaluations, 0);
			submit_counter("pf_bytes", rulestring,
			    (unsigned long long)(rule.packets[0] + rule.packets[1]), 0);
#else
			printf("Rule-Number: %i\n", rule.nr);
			printf("Rule: %s\n", rulestring);
			printf("States cur: %-6u\n", rule.states_cur);
			printf("States tot: %-6u\n", rule.states_tot);
			printf("Evaluations: %-8llu\n",
			    (unsigned long long)rule.evaluations);
			printf("Bytes: %-10llu\n",
			    (unsigned long long)(rule.packets[0] + rule.packets[1]));
			printf("\n");
#endif
		}
	}

	close(dev);
	return (0);
}

#ifdef TEST
int
main(int argc, char *argv[])
{
	if (pfrules_init())
		err(1, "pfrules_init");
	if (pfrules_read())
		err(1, "pfrules_read");
	return (0);
}
#else
void module_register(void) {
	plugin_register_init("pfrules", pfrules_init);
	plugin_register_read("pfrules", pfrules_read);
}
#endif