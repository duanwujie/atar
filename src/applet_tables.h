#define NUM_APPLETS 1
#define SINGLE_APPLET_STR "tar"
//#define SINGLE_APPLET_MAIN tar_mains

const char applet_names[] ALIGN1 = ""
"tar" "\0"
;

#ifndef SKIP_applet_main
int (*const applet_main[])(int argc, char **argv) = {
};
#endif

const uint16_t applet_nameofs[] ALIGN2 = {
0x0000,
};

const uint8_t applet_install_loc[] ALIGN1 = {
0x01,
};
