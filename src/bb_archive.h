/* vi: set sw=4 ts=4: */
#ifndef UNARCHIVE_H
#define UNARCHIVE_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

enum {
#if BB_BIG_ENDIAN
	COMPRESS_MAGIC = 0x1f9d,
	GZIP_MAGIC  = 0x1f8b,
	BZIP2_MAGIC = 256 * 'B' + 'Z',
	/* .xz signature: 0xfd, '7', 'z', 'X', 'Z', 0x00 */
	/* More info at: http://tukaani.org/xz/xz-file-format.txt */
	XZ_MAGIC1   = 256 * 0xfd + '7',
	XZ_MAGIC2   = 256 * (unsigned)(256 * (256 * 'z' + 'X') + 'Z') + 0,
	/* Different form: 32 bits, then 16 bits: */
	/* (unsigned) cast suppresses "integer overflow in expression" warning */
	XZ_MAGIC1a  = 256 * (unsigned)(256 * (256 * 0xfd + '7') + 'z') + 'X',
	XZ_MAGIC2a  = 256 * 'Z' + 0,
#else
	COMPRESS_MAGIC = 0x9d1f,
	GZIP_MAGIC  = 0x8b1f,
	BZIP2_MAGIC = 'B' + 'Z' * 256,
	XZ_MAGIC1   = 0xfd + '7' * 256,
	XZ_MAGIC2   = 'z' + ('X' + ('Z' + 0 * 256) * 256) * 256,
	XZ_MAGIC1a  = 0xfd + ('7' + ('z' + 'X' * 256) * 256) * 256,
	XZ_MAGIC2a  = 'Z' + 0 * 256,
#endif
};

typedef struct file_header_t {
	char *name;
	char *link_target;
#if ENABLE_FEATURE_TAR_UNAME_GNAME
	char *tar__uname;
	char *tar__gname;
#endif
	off_t size;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t mtime;
	dev_t device;
} file_header_t;

struct hardlinks_t;

typedef struct archive_handle_t {
	unsigned ah_flags;/* Flags. 1st since it is most used member */
	int src_fd;/* The raw stream as read from disk or stdin */
	/* Define if the header and data component should be processed */
	char FAST_FUNC (*filter)(struct archive_handle_t *);
	llist_t *accept;/* List of files that have been accepted */
	llist_t *reject;/* List of files that have been rejected */
	llist_t *passed;/* List of files that have successfully been worked on */

	file_header_t *file_header;/* Currently processed file's header */

	
	/* Process the header component, e.g. tar -t */
	void FAST_FUNC (*action_header)(const file_header_t *);

	/* Process the data component, e.g. extract to filesystem */
	void FAST_FUNC (*action_data)(struct archive_handle_t *);

	void FAST_FUNC (*seek)(int fd, off_t amount);/* Function that skips data */
	off_t offset;/* Count processed bytes */

	/* Archiver specific. Can make it a union if it ever gets big */
#define PAX_NEXT_FILE 0
#define PAX_GLOBAL    1
#if ENABLE_TAR || ENABLE_DPKG || ENABLE_DPKG_DEB
	smallint tar__end;
# if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
	char* tar__longname;
	char* tar__linkname;
# endif
# if ENABLE_FEATURE_TAR_TO_COMMAND
	char* tar__to_command;
	const char* tar__to_command_shell;
# endif
#endif
} archive_handle_t;
/* bits in ah_flags */
#define ARCHIVE_RESTORE_DATE        (1 << 0)
#define ARCHIVE_CREATE_LEADING_DIRS (1 << 1)
#define ARCHIVE_UNLINK_OLD          (1 << 2)
#define ARCHIVE_EXTRACT_QUIET       (1 << 3)
#define ARCHIVE_EXTRACT_NEWER       (1 << 4)
#define ARCHIVE_DONT_RESTORE_OWNER  (1 << 5)
#define ARCHIVE_DONT_RESTORE_PERM   (1 << 6)
#define ARCHIVE_NUMERIC_OWNER       (1 << 7)
#define ARCHIVE_O_TRUNC             (1 << 8)
#define ARCHIVE_REMEMBER_NAMES      (1 << 9)
#if ENABLE_RPM
#define ARCHIVE_REPLACE_VIA_RENAME  (1 << 10)
#endif


/*
 * tar 包格式
 * [tar_header1][file1][tar_header-1][file2][tar-header]
 */

/* POSIX tar Header Block, from POSIX 1003.1-1990  */
#define TAR_BLOCK_SIZE 512
#define NAME_SIZE      100
#define NAME_SIZE_STR "100"
/**
 * @brief Tar Format
 */
typedef struct tar_header_t {     /* byte offset */
	char name[NAME_SIZE];     /*   0-99 */
	char mode[8];             /* 100-107 */
	char uid[8];              /* 108-115 */
	char gid[8];              /* 116-123 */
	char size[12];            /* 124-135 */
	char mtime[12];           /* 136-147 */
	char chksum[8];           /* 148-155 */
	char typeflag;            /* 156-156 */
	char linkname[NAME_SIZE]; /* 157-256 */
	/* POSIX:   "ustar" NUL "00" */
	/* GNU tar: "ustar  " NUL */
	/* Normally it's defined as magic[6] followed by
	 * version[2], but we put them together to save code.
	 */
	char magic[8];            /* 257-264 */
	char uname[32];           /* 265-296 */
	char gname[32];           /* 297-328 */
	char devmajor[8];         /* 329-336 */
	char devminor[8];         /* 337-344 */
	char prefix[155];         /* 345-499 */
	char padding[12];         /* 500-512 (pad to exactly TAR_BLOCK_SIZE) */
} tar_header_t;




archive_handle_t *init_handle(void) FAST_FUNC;

char filter_accept_all(archive_handle_t *archive_handle) FAST_FUNC;
char filter_accept_list(archive_handle_t *archive_handle) FAST_FUNC;
char filter_accept_list_reassign(archive_handle_t *archive_handle) FAST_FUNC;
char filter_accept_reject_list(archive_handle_t *archive_handle) FAST_FUNC;

void unpack_ar_archive(archive_handle_t *ar_archive) FAST_FUNC;

void data_skip(archive_handle_t *archive_handle) FAST_FUNC;
void data_extract_all(archive_handle_t *archive_handle) FAST_FUNC;
void data_extract_to_stdout(archive_handle_t *archive_handle) FAST_FUNC;
void data_extract_to_command(archive_handle_t *archive_handle) FAST_FUNC;

void header_skip(const file_header_t *file_header) FAST_FUNC;
void header_list(const file_header_t *file_header) FAST_FUNC;
void header_verbose_list(const file_header_t *file_header) FAST_FUNC;

char get_header_tar(archive_handle_t *archive_handle) FAST_FUNC;

void seek_by_jump(int fd, off_t amount) FAST_FUNC;
void seek_by_read(int fd, off_t amount) FAST_FUNC;

const char *strip_unsafe_prefix(const char *str) FAST_FUNC;

void data_align(archive_handle_t *archive_handle, unsigned boundary) FAST_FUNC;
const llist_t *find_list_entry(const llist_t *list, const char *filename) FAST_FUNC;
const llist_t *find_list_entry2(const llist_t *list, const char *filename) FAST_FUNC;

/* A bit of bunzip2 internals are exposed for compressed help support: */
typedef struct bunzip_data bunzip_data;
int start_bunzip(bunzip_data **bdp, int in_fd, const void *inbuf, int len) FAST_FUNC;
/* NB: read_bunzip returns < 0 on error, or the number of *unfilled* bytes
 * in outbuf. IOW: on EOF returns len ("all bytes are not filled"), not 0: */
int read_bunzip(bunzip_data *bd, char *outbuf, int len) FAST_FUNC;
void dealloc_bunzip(bunzip_data *bd) FAST_FUNC;

/* Meaning and direction (input/output) of the fields are transformer-specific */
typedef struct transformer_aux_data_t {
	smallint check_signature; /* most often referenced member */
	off_t    bytes_out;
	off_t    bytes_in;  /* used in unzip code only: needs to know packed size */
	uint32_t crc32;
	time_t   mtime;     /* gunzip code may set this on exit */
} transformer_aux_data_t;

void init_transformer_aux_data(transformer_aux_data_t *aux) FAST_FUNC;
int FAST_FUNC check_signature16(transformer_aux_data_t *aux, int src_fd, unsigned magic16) FAST_FUNC;

IF_DESKTOP(long long) int inflate_unzip(transformer_aux_data_t *aux, int src_fd, int dst_fd) FAST_FUNC;
IF_DESKTOP(long long) int unpack_Z_stream(transformer_aux_data_t *aux, int src_fd, int dst_fd) FAST_FUNC;
IF_DESKTOP(long long) int unpack_gz_stream(transformer_aux_data_t *aux, int src_fd, int dst_fd) FAST_FUNC;
IF_DESKTOP(long long) int unpack_bz2_stream(transformer_aux_data_t *aux, int src_fd, int dst_fd) FAST_FUNC;
IF_DESKTOP(long long) int unpack_lzma_stream(transformer_aux_data_t *aux, int src_fd, int dst_fd) FAST_FUNC;
IF_DESKTOP(long long) int unpack_xz_stream(transformer_aux_data_t *aux, int src_fd, int dst_fd) FAST_FUNC;

char* append_ext(char *filename, const char *expected_ext) FAST_FUNC;
int bbunpack(char **argv,
		IF_DESKTOP(long long) int FAST_FUNC (*unpacker)(transformer_aux_data_t *aux),
		char* FAST_FUNC (*make_new_name)(char *filename, const char *expected_ext),
		const char *expected_ext
) FAST_FUNC;

void check_errors_in_children(int signo);
#if BB_MMU
void open_transformer(int fd,
	int check_signature,
	IF_DESKTOP(long long) int FAST_FUNC (*transformer)(transformer_aux_data_t *aux, int src_fd, int dst_fd)
) FAST_FUNC;
#define open_transformer_with_sig(fd, transformer, transform_prog) open_transformer((fd), 1, (transformer))
#define open_transformer_with_no_sig(fd, transformer)              open_transformer((fd), 0, (transformer))
#else
void open_transformer(int fd, const char *transform_prog) FAST_FUNC;
#define open_transformer_with_sig(fd, transformer, transform_prog) open_transformer((fd), (transform_prog))
/* open_transformer_with_no_sig() does not exist on NOMMU */
#endif


POP_SAVED_FUNCTION_VISIBILITY

#endif
