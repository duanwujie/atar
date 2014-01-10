/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) tons of folks.  Tracking down who wrote what
 * isn't something I'm going to worry about...  If you wrote something
 * here, please feel free to acknowledge your work.
 *
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell
 * Permission has been granted to redistribute this code under GPL.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* We are trying to not use printf, this benefits the case when selected
 * applets are really simple. Example:
 *
 * $ ./busybox
 * ...
 * Currently defined functions:
 *         basename, false, true
 *
 * $ size busybox
 *    text    data     bss     dec     hex filename
 *    4473      52      72    4597    11f5 busybox
 *
 * FEATURE_INSTALLER or FEATURE_SUID will still link printf routines in. :(
 */
#include "busybox.h"

/*
#if !(defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
    || defined(__APPLE__) \
    )
# include <malloc.h>  for mallopt
#endif
*/


/* Declare <applet>_main() */
#define PROTOTYPES
#include "applets.h"
#undef PROTOTYPES

/* Include generated applet names, pointers to <applet>_main, etc */
#include "applet_tables.h"
/* ...and if applet_tables generator says we have only one applet... */
#ifdef SINGLE_APPLET_MAIN
# undef ENABLE_FEATURE_INDIVIDUAL
# define ENABLE_FEATURE_INDIVIDUAL 1
# undef IF_FEATURE_INDIVIDUAL
# define IF_FEATURE_INDIVIDUAL(...) __VA_ARGS__
#endif

#include "usage_compressed.h"


#if ENABLE_SHOW_USAGE && !ENABLE_FEATURE_COMPRESS_USAGE
static const char usage_messages[] ALIGN1 = UNPACKED_USAGE;
#else
# define usage_messages 0
#endif


#if ENABLE_FEATURE_COMPRESS_USAGE

static const char packed_usage[] ALIGN1 = { PACKED_USAGE };
# include "bb_archive.h"
static const char *unpack_usage_messages(void)
{
	char *outbuf = NULL;
	bunzip_data *bd;
	int i;

	i = start_bunzip(&bd,
			/* src_fd: */ -1,
			/* inbuf:  */ packed_usage,
			/* len:    */ sizeof(packed_usage));
	/* read_bunzip can longjmp to start_bunzip, and ultimately
	 * end up here with i != 0 on read data errors! Not trivial */
	if (!i) {
		/* Cannot use xmalloc: will leak bd in NOFORK case! */
		outbuf = (char *)malloc_or_warn(sizeof(UNPACKED_USAGE));
		if (outbuf)
			read_bunzip(bd, outbuf, sizeof(UNPACKED_USAGE));
	}
	dealloc_bunzip(bd);
	return outbuf;
}
# define dealloc_usage_messages(s) free(s)

#else

# define unpack_usage_messages() usage_messages
# define dealloc_usage_messages(s) ((void)(s))

#endif /* FEATURE_COMPRESS_USAGE */





void FAST_FUNC bb_show_usage(void)
{
	if (ENABLE_SHOW_USAGE) {
#ifdef SINGLE_APPLET_STR
		/* Imagine that this applet is "true". Dont suck in printf! */
		const char *usage_string = unpack_usage_messages();

		if (*usage_string == '\b') {
			full_write2_str("No help available.\n\n");
		} else {
			full_write2_str("Usage: "SINGLE_APPLET_STR" ");
			full_write2_str(usage_string);
			full_write2_str("\n\n");
		}
		if (ENABLE_FEATURE_CLEAN_UP)
			dealloc_usage_messages((char*)usage_string);
#else
		const char *p;
		const char *usage_string = p = unpack_usage_messages();
		int ap = find_applet_by_name(applet_name);

		if (ap < 0) /* never happens, paranoia */
			xfunc_die();
		while (ap) {
			while (*p++) continue;
			ap--;
		}
		full_write2_str(bb_banner);
		full_write2_str(" multi-call binary.\n");
		if (*p == '\b')
			full_write2_str("\nNo help available.\n\n");
		else {
			full_write2_str("\nUsage: ");
			full_write2_str(applet_name);
			full_write2_str(" ");
			full_write2_str(p);
			full_write2_str("\n\n");
		}
		if (ENABLE_FEATURE_CLEAN_UP)
			dealloc_usage_messages((char*)usage_string);
#endif
	}
	xfunc_die();
}

#if NUM_APPLETS > 8
static int applet_name_compare(const void *name, const void *idx)
{
	int i = (int)(ptrdiff_t)idx - 1;
	return strcmp(name, APPLET_NAME(i));
}
#endif
int FAST_FUNC find_applet_by_name(const char *name)
{
#if NUM_APPLETS > 8
	/* Do a binary search to find the applet entry given the name. */
	const char *p;
	p = bsearch(name, (void*)(ptrdiff_t)1, ARRAY_SIZE(applet_main), 1, applet_name_compare);
	/*
	 * if (!p) return -1;
	 * ^^^^^^^^^^^^^^^^^^ the code below will do this if p == NULL :)
	 */
	return (int)(ptrdiff_t)p - 1;
#else
	/* A version which does not pull in bsearch */
	int i = 0;
	const char *p = applet_names;
	while (i < NUM_APPLETS) {
		if (strcmp(name, p) == 0)
			return i;
		p += strlen(p) + 1;
		i++;
	}
	return -1;
#endif
}




/* The code below can well be in applets/applets.c, as it is used only
 * for busybox binary, not "individual" binaries.
 * However, keeping it here and linking it into libbusybox.so
 * (together with remaining tiny applets/applets.o)
 * makes it possible to avoid --whole-archive at link time.
 * This makes (shared busybox) + libbusybox smaller.
 * (--gc-sections would be even better....)
 */

const char *applet_name;
#if !BB_MMU
bool re_execed;
#endif


/* If not built as a single-applet executable... */
#if !defined(SINGLE_APPLET_MAIN)

IF_FEATURE_SUID(static uid_t ruid;)  /* real uid */







# if ENABLE_FEATURE_INSTALLER
static const char usr_bin [] ALIGN1 = "/usr/bin/";
static const char usr_sbin[] ALIGN1 = "/usr/sbin/";
static const char *const install_dir[] = {
	&usr_bin [8], /* "/" */
	&usr_bin [4], /* "/bin/" */
	&usr_sbin[4]  /* "/sbin/" */
#  if !ENABLE_INSTALL_NO_USR
	,usr_bin
	,usr_sbin
#  endif
};

/* create (sym)links for each applet */
static void install_links(const char *busybox, int use_symbolic_links,
		char *custom_install_dir)
{
	/* directory table
	 * this should be consistent w/ the enum,
	 * busybox.h::bb_install_loc_t, or else... */
	int (*lf)(const char *, const char *);
	char *fpc;
	unsigned i;
	int rc;

	lf = link;
	if (use_symbolic_links)
		lf = symlink;

	for (i = 0; i < ARRAY_SIZE(applet_main); i++) {
		fpc = concat_path_file(
				custom_install_dir ? custom_install_dir : install_dir[APPLET_INSTALL_LOC(i)],
				APPLET_NAME(i));
		// debug: bb_error_msg("%slinking %s to busybox",
		//		use_symbolic_links ? "sym" : "", fpc);
		rc = lf(busybox, fpc);
		if (rc != 0 && errno != EEXIST) {
			bb_simple_perror_msg(fpc);
		}
		free(fpc);
	}
}
# else
#  define install_links(x,y,z) ((void)0)
# endif







#endif /* !defined(SINGLE_APPLET_MAIN) */



#if 0
int main(int argc UNUSED_PARAM, char **argv) //dwj
{

	/* Only one applet is selected in .config */
	printf("do cao \n");
	if (argv[1] && strncmp(argv[0], "busybox", 7) == 0) {
		/* "busybox <applet> <params>" should still work as expected */
		argv++;
	}
	/* applet_names in this case is just "applet\0\0" */
	lbb_prepare(applet_names IF_FEATURE_INDIVIDUAL(, argv));
	return SINGLE_APPLET_MAIN(argc, argv);

}
#endif

