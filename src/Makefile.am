#AM_CFLAGS = -std=c99 -Wall -Wextra -Werror -Wstrict-prototypes -Wredundant-decls -Wshadow -pedantic -fno-strict-aliasing -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -O2 -D_SVID_SOURCE

AM_CFLAGS = -Wunused-function -Wall

#if ENABLE_DEBUG_BUILD
#AM_CFLAGS += -g 
#else
#AM_CFLAGS += -DNDEBUG
#endif


INCLUDE = -I.
sbin_PROGRAMS = atar

TAR_PREFIX :=.
atar_SOURCES =     $(TAR_PREFIX)/tar.cpp \
                   $(TAR_PREFIX)/xfuncs_printf.cpp \
                   $(TAR_PREFIX)/llist.cpp \
                   $(TAR_PREFIX)/verror_msg.cpp \
                   $(TAR_PREFIX)/wfopen_input.cpp \
                   $(TAR_PREFIX)/last_char_is.cpp \
                   $(TAR_PREFIX)/messages.cpp \
                   $(TAR_PREFIX)/xfunc_die.cpp \
                   $(TAR_PREFIX)/default_error_retval.cpp \
                   $(TAR_PREFIX)/wfopen.cpp \
                   $(TAR_PREFIX)/perror_msg.cpp \
                   $(TAR_PREFIX)/get_line_from_file.cpp \
                   $(TAR_PREFIX)/procps.cpp \
                   $(TAR_PREFIX)/xreadlink.cpp \
                   $(TAR_PREFIX)/safe_strncpy.cpp \
                   $(TAR_PREFIX)/get_header_tar.cpp \
                   $(TAR_PREFIX)/copyfd.cpp \
                   $(TAR_PREFIX)/find_list_entry.cpp \
                   $(TAR_PREFIX)/header_verbose_list.cpp\
                   $(TAR_PREFIX)/recursive_action.cpp \
                   $(TAR_PREFIX)/init_handle.cpp \
                   $(TAR_PREFIX)/header_list.cpp \
                   $(TAR_PREFIX)/seek_by_read.cpp \
                   $(TAR_PREFIX)/data_extract_all.cpp \
                   $(TAR_PREFIX)/filter_accept_reject_list.cpp \
                   $(TAR_PREFIX)/data_extract_to_stdout.cpp \
                   $(TAR_PREFIX)/data_skip.cpp \
                   $(TAR_PREFIX)/make_directory.cpp \
                   $(TAR_PREFIX)/seek_by_jump.cpp \
                   $(TAR_PREFIX)/filter_accept_all.cpp\
                   $(TAR_PREFIX)/header_skip.cpp \
                   $(TAR_PREFIX)/concat_subpath_file.cpp \
                   $(TAR_PREFIX)/concat_path_file.cpp \
                   $(TAR_PREFIX)/mode_string.cpp \
                   $(TAR_PREFIX)/xfuncs.cpp \
                   $(TAR_PREFIX)/full_write.cpp \
                   $(TAR_PREFIX)/xatonum.cpp\
                   $(TAR_PREFIX)/safe_write.cpp \
                   $(TAR_PREFIX)/read_printf.cpp \
                   $(TAR_PREFIX)/read.cpp \
                   $(TAR_PREFIX)/bb_strtonum.cpp\
                   $(TAR_PREFIX)/data_align.cpp \
                   $(TAR_PREFIX)/safe_poll.cpp \
                   $(TAR_PREFIX)/get_last_path_component.cpp\
                   $(TAR_PREFIX)/platform.cpp \
                   $(TAR_PREFIX)/xrealloc_vector.cpp \
                   $(TAR_PREFIX)/bb_pwd.cpp\
                   $(TAR_PREFIX)/data_extract_to_command.cpp \
                   $(TAR_PREFIX)/appletlib.cpp

