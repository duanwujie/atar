#include "busybox.h"



/* Declare <applet>_main() */
#define PROTOTYPES
#include "applets.h"
#undef PROTOTYPES

/* Include generated applet names, pointers to <applet>_main, etc */
#include "applet_tables.h"
#include "usage_compressed.h"


static const char usage_messages[] ALIGN1 = UNPACKED_USAGE;
# define unpack_usage_messages() usage_messages
# define dealloc_usage_messages(s) ((void)(s))






void FAST_FUNC bb_show_usage(void)
{
	if (ENABLE_SHOW_USAGE) {
		/* Imagine that this applet is "true". Dont suck in printf! */
		const char *usage_string = unpack_usage_messages();

		if (*usage_string == '\b') {
			full_write2_str("No help available.\n\n");
		} else {
			full_write2_str("Usage: "SINGLE_APPLET_STR" ");
			full_write2_str(usage_string);
			full_write2_str("\n\n");
		}
	}
	xfunc_die();
}

int FAST_FUNC find_applet_by_name(const char *name)
{
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
