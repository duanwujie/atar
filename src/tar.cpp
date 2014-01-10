#include <fnmatch.h>
#include "libbb.h"
#include "bb_archive.h"
/* FIXME: Stop using this non-standard feature */
#ifndef FNM_LEADING_DIR
# define FNM_LEADING_DIR 0
#endif


//#define DBG(fmt, ...) bb_error_msg("%s: " fmt, __func__, ## __VA_ARGS__)
#define DBG(...) ((void)0)


#define block_buf bb_common_bufsiz1





/*
** writeTarFile(), writeFileToTarball(), and writeTarHeader() are
** the only functions that deal with the HardLinkInfo structure.
** Even these functions use the xxxHardLinkInfo() functions.
*/
typedef struct HardLinkInfo {
	struct HardLinkInfo *next; /* Next entry in list */
	dev_t dev;                 /* Device number */
	ino_t ino;                 /* Inode number */
//	short linkCount;           /* (Hard) Link Count */
	char name[1];              /* Start of filename (must be last) */
} HardLinkInfo;

/* Some info to be carried along when creating a new tarball */
typedef struct TarBallInfo {
	int tarFd;                      /* Open-for-write file descriptor
	                                 * for the tarball */
	int verboseFlag;                /* Whether to print extra stuff or not */
	const llist_t *excludeList;     /* List of files to not include */
	HardLinkInfo *hlInfoHead;       /* Hard Link Tracking Information */
	HardLinkInfo *hlInfo;           /* Hard Link Info for the current file */
//TODO: save only st_dev + st_ino
	struct stat tarFileStatBuf;     /* Stat info for the tarball, letting
	                                 * us know the inode and device that the
	                                 * tarball lives, so we can avoid trying
	                                 * to include the tarball into itself */
} TarBallInfo;

/* A nice enum with all the possible tar file content types */
enum {
	REGTYPE = '0',		/* regular file */
	REGTYPE0 = '\0',	/* regular file (ancient bug compat) */
	LNKTYPE = '1',		/* hard link */
	SYMTYPE = '2',		/* symbolic link */
	CHRTYPE = '3',		/* character special */
	BLKTYPE = '4',		/* block special */
	DIRTYPE = '5',		/* directory */
	FIFOTYPE = '6',		/* FIFO special */
	CONTTYPE = '7',		/* reserved */
	GNULONGLINK = 'K',	/* GNU long (>100 chars) link name */
	GNULONGNAME = 'L',	/* GNU long (>100 chars) file name */
};

/* Might be faster (and bigger) if the dev/ino were stored in numeric order;) */
static void addHardLinkInfo(HardLinkInfo **hlInfoHeadPtr,
					struct stat *statbuf,
					const char *fileName)
{
	/* Note: hlInfoHeadPtr can never be NULL! */
	HardLinkInfo *hlInfo;

	hlInfo = (HardLinkInfo *)xmalloc(sizeof(HardLinkInfo) + strlen(fileName));
	hlInfo->next = *hlInfoHeadPtr;
	*hlInfoHeadPtr = hlInfo;
	hlInfo->dev = statbuf->st_dev;
	hlInfo->ino = statbuf->st_ino;
//	hlInfo->linkCount = statbuf->st_nlink;
	strcpy(hlInfo->name, fileName);
}

static void freeHardLinkInfo(HardLinkInfo **hlInfoHeadPtr)
{
	HardLinkInfo *hlInfo;
	HardLinkInfo *hlInfoNext;

	if (hlInfoHeadPtr) {
		hlInfo = *hlInfoHeadPtr;
		while (hlInfo) {
			hlInfoNext = hlInfo->next;
			free(hlInfo);
			hlInfo = hlInfoNext;
		}
		*hlInfoHeadPtr = NULL;
	}
}

/* Might be faster (and bigger) if the dev/ino were stored in numeric order ;) */
static HardLinkInfo *findHardLinkInfo(HardLinkInfo *hlInfo, struct stat *statbuf)
{
	while (hlInfo) {
		if (statbuf->st_ino == hlInfo->ino
		 && statbuf->st_dev == hlInfo->dev
		) {
			DBG("found hardlink:'%s'", hlInfo->name);
			break;
		}
		hlInfo = hlInfo->next;
	}
	return hlInfo;
}

/* Put an octal string into the specified buffer.
 * The number is zero padded and possibly null terminated.
 * Stores low-order bits only if whole value does not fit. */
static void putOctal(char *cp, int len, off_t value)
{
	char tempBuffer[sizeof(off_t)*3 + 1];
	char *tempString = tempBuffer;
	int width;

	width = sprintf(tempBuffer, "%0*"OFF_FMT"o", len, value);
	tempString += (width - len);

	/* If string has leading zeroes, we can drop one */
	/* and field will have trailing '\0' */
	/* (increases chances of compat with other tars) */
	if (tempString[0] == '0')
		tempString++;

	/* Copy the string to the field */
	memcpy(cp, tempString, len);
}
#define PUT_OCTAL(a, b) putOctal((a), sizeof(a), (b))

static void chksum_and_xwrite(int fd, struct tar_header_t* hp)
{
	/* POSIX says that checksum is done on unsigned bytes
	 * (Sun and HP-UX gets it wrong... more details in
	 * GNU tar source) */
	const unsigned char *cp;
	int chksum, size;

	strcpy(hp->magic, "ustar  ");

	/* Calculate and store the checksum (i.e., the sum of all of the bytes of
	 * the header).  The checksum field must be filled with blanks for the
	 * calculation.  The checksum field is formatted differently from the
	 * other fields: it has 6 digits, a null, then a space -- rather than
	 * digits, followed by a null like the other fields... */
	memset(hp->chksum, ' ', sizeof(hp->chksum));
	cp = (const unsigned char *) hp;
	chksum = 0;
	size = sizeof(*hp);
	do { chksum += *cp++; } while (--size);
	putOctal(hp->chksum, sizeof(hp->chksum)-1, chksum);

	/* Now write the header out to disk */
	xwrite(fd, hp, sizeof(*hp));
}

#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
static void writeLongname(int fd, int type, const char *name, int dir)
{
	static const struct {
		char mode[8];             /* 100-107 */
		char uid[8];              /* 108-115 */
		char gid[8];              /* 116-123 */
		char size[12];            /* 124-135 */
		char mtime[12];           /* 136-147 */
	} prefilled = {
		"0000000",
		"0000000",
		"0000000",
		"00000000000",
		"00000000000",
	};
	struct tar_header_t header;
	int size;

	dir = !!dir; /* normalize: 0/1 */
	size = strlen(name) + 1 + dir; /* GNU tar uses strlen+1 */
	/* + dir: account for possible '/' */

	memset(&header, 0, sizeof(header));
	strcpy(header.name, "././@LongLink");
	memcpy(header.mode, prefilled.mode, sizeof(prefilled));
	PUT_OCTAL(header.size, size);
	header.typeflag = type;
	chksum_and_xwrite(fd, &header);

	/* Write filename[/] and pad the block. */
	/* dir=0: writes 'name<NUL>', pads */
	/* dir=1: writes 'name', writes '/<NUL>', pads */
	dir *= 2;
	xwrite(fd, name, size - dir);
	xwrite(fd, "/", dir);
	size = (-size) & (TAR_BLOCK_SIZE-1);
	memset(&header, 0, size);
	xwrite(fd, &header, size);
}
#endif

/* Write out a tar header for the specified file/directory/whatever */
static int writeTarHeader(struct TarBallInfo *tbInfo,
		const char *header_name, const char *fileName, struct stat *statbuf)
{
	struct tar_header_t header;

	memset(&header, 0, sizeof(header));

	strncpy(header.name, header_name, sizeof(header.name));

	/* POSIX says to mask mode with 07777. */
	PUT_OCTAL(header.mode, statbuf->st_mode & 07777);
	PUT_OCTAL(header.uid, statbuf->st_uid);
	PUT_OCTAL(header.gid, statbuf->st_gid);
	memset(header.size, '0', sizeof(header.size)-1); /* Regular file size is handled later */
	/* users report that files with negative st_mtime cause trouble, so: */
	PUT_OCTAL(header.mtime, statbuf->st_mtime >= 0 ? statbuf->st_mtime : 0);

	/* Enter the user and group names */
	safe_strncpy(header.uname, get_cached_username(statbuf->st_uid), sizeof(header.uname));
	safe_strncpy(header.gname, get_cached_groupname(statbuf->st_gid), sizeof(header.gname));

	if (tbInfo->hlInfo) {
		/* This is a hard link */
		header.typeflag = LNKTYPE;
		strncpy(header.linkname, tbInfo->hlInfo->name,
				sizeof(header.linkname));
#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
		/* Write out long linkname if needed */
		if (header.linkname[sizeof(header.linkname)-1])
			writeLongname(tbInfo->tarFd, GNULONGLINK,
					tbInfo->hlInfo->name, 0);
#endif
	} else if (S_ISLNK(statbuf->st_mode)) {
		char *lpath = xmalloc_readlink_or_warn(fileName);
		if (!lpath)
			return FALSE;
		header.typeflag = SYMTYPE;
		strncpy(header.linkname, lpath, sizeof(header.linkname));
#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
		/* Write out long linkname if needed */
		if (header.linkname[sizeof(header.linkname)-1])
			writeLongname(tbInfo->tarFd, GNULONGLINK, lpath, 0);
#else
		/* If it is larger than 100 bytes, bail out */
		if (header.linkname[sizeof(header.linkname)-1]) {
			free(lpath);
			bb_error_msg("names longer than "NAME_SIZE_STR" chars not supported");
			return FALSE;
		}
#endif
		free(lpath);
	} else if (S_ISDIR(statbuf->st_mode)) {
		header.typeflag = DIRTYPE;
		/* Append '/' only if there is a space for it */
		if (!header.name[sizeof(header.name)-1])
			header.name[strlen(header.name)] = '/';
	} else if (S_ISCHR(statbuf->st_mode)) {
		header.typeflag = CHRTYPE;
		PUT_OCTAL(header.devmajor, major(statbuf->st_rdev));
		PUT_OCTAL(header.devminor, minor(statbuf->st_rdev));
	} else if (S_ISBLK(statbuf->st_mode)) {
		header.typeflag = BLKTYPE;
		PUT_OCTAL(header.devmajor, major(statbuf->st_rdev));
		PUT_OCTAL(header.devminor, minor(statbuf->st_rdev));
	} else if (S_ISFIFO(statbuf->st_mode)) {
		header.typeflag = FIFOTYPE;
	} else if (S_ISREG(statbuf->st_mode)) {
		/* header.size field is 12 bytes long */
		/* Does octal-encoded size fit? */
		uoff_t filesize = statbuf->st_size;
		if (sizeof(filesize) <= 4
		 || filesize <= (uoff_t)0777777777777LL
		) {
			PUT_OCTAL(header.size, filesize);
		}
		/* Does base256-encoded size fit?
		 * It always does unless off_t is wider than 64 bits.
		 */
		else if (ENABLE_FEATURE_TAR_GNU_EXTENSIONS
#if ULLONG_MAX > 0xffffffffffffffffLL /* 2^64-1 */
		 && (filesize <= 0x3fffffffffffffffffffffffLL)
#endif
		) {
			/* GNU tar uses "base-256 encoding" for very large numbers.
			 * Encoding is binary, with highest bit always set as a marker
			 * and sign in next-highest bit:
			 * 80 00 .. 00 - zero
			 * bf ff .. ff - largest positive number
			 * ff ff .. ff - minus 1
			 * c0 00 .. 00 - smallest negative number
			 */
			char *p8 = header.size + sizeof(header.size);
			do {
				*--p8 = (uint8_t)filesize;
				filesize >>= 8;
			} while (p8 != header.size);
			*p8 |= 0x80;
		} else {
			bb_error_msg_and_die("can't store file '%s' "
				"of size %"OFF_FMT"u, aborting",
				fileName, statbuf->st_size);
		}
		header.typeflag = REGTYPE;
	} else {
		bb_error_msg("%s: unknown file type", fileName);
		return FALSE;
	}

#if ENABLE_FEATURE_TAR_GNU_EXTENSIONS
	/* Write out long name if needed */
	/* (we, like GNU tar, output long linkname *before* long name) */
	if (header.name[sizeof(header.name)-1])
		writeLongname(tbInfo->tarFd, GNULONGNAME,
				header_name, S_ISDIR(statbuf->st_mode));
#endif

	/* Now write the header out to disk */
	chksum_and_xwrite(tbInfo->tarFd, &header);

	/* Now do the verbose thing (or not) */
	if (tbInfo->verboseFlag) {
		FILE *vbFd = stdout;

		/* If archive goes to stdout, verbose goes to stderr */
		if (tbInfo->tarFd == STDOUT_FILENO)
			vbFd = stderr;
		/* GNU "tar cvvf" prints "extended" listing a-la "ls -l" */
		/* We don't have such excesses here: for us "v" == "vv" */
		/* '/' is probably a GNUism */
		fprintf(vbFd, "%s%s\n", header_name,
				S_ISDIR(statbuf->st_mode) ? "/" : "");
	}

	return TRUE;
}


# define exclude_file(excluded_files, file) 0

static int FAST_FUNC writeFileToTarball(const char *fileName, struct stat *statbuf,
			void *userData, int depth UNUSED_PARAM)
{
	struct TarBallInfo *tbInfo = (struct TarBallInfo *) userData;
	const char *header_name;
	int inputFileFd = -1;

	DBG("writeFileToTarball('%s')", fileName);

	/* Strip leading '/' and such (must be before memorizing hardlink's name) */
	header_name = strip_unsafe_prefix(fileName);

	if (header_name[0] == '\0')
		return TRUE;

	/* It is against the rules to archive a socket */
	if (S_ISSOCK(statbuf->st_mode)) {
		bb_error_msg("%s: socket ignored", fileName);
		return TRUE;
	}

	/*
	 * Check to see if we are dealing with a hard link.
	 * If so -
	 * Treat the first occurance of a given dev/inode as a file while
	 * treating any additional occurances as hard links.  This is done
	 * by adding the file information to the HardLinkInfo linked list.
	 */
	tbInfo->hlInfo = NULL;
	if (!S_ISDIR(statbuf->st_mode) && statbuf->st_nlink > 1) {
		DBG("'%s': st_nlink > 1", header_name);
		tbInfo->hlInfo = findHardLinkInfo(tbInfo->hlInfoHead, statbuf);
		if (tbInfo->hlInfo == NULL) {
			DBG("'%s': addHardLinkInfo", header_name);
			addHardLinkInfo(&tbInfo->hlInfoHead, statbuf, header_name);
		}
	}

	/* It is a bad idea to store the archive we are in the process of creating,
	 * so check the device and inode to be sure that this particular file isn't
	 * the new tarball */
	if (tbInfo->tarFileStatBuf.st_dev == statbuf->st_dev
	 && tbInfo->tarFileStatBuf.st_ino == statbuf->st_ino
	) {
		bb_error_msg("%s: file is the archive; skipping", fileName);
		return TRUE;
	}

	if (exclude_file(tbInfo->excludeList, header_name))
		return SKIP;

#if !ENABLE_FEATURE_TAR_GNU_EXTENSIONS
	if (strlen(header_name) >= NAME_SIZE) {
		bb_error_msg("names longer than "NAME_SIZE_STR" chars not supported");
		return TRUE;
	}
#endif

	/* Is this a regular file? */
	if (tbInfo->hlInfo == NULL && S_ISREG(statbuf->st_mode)) {
		/* open the file we want to archive, and make sure all is well */
		inputFileFd = open_or_warn(fileName, O_RDONLY);
		if (inputFileFd < 0) {
			return FALSE;
		}
	}

	/* Add an entry to the tarball */
	if (writeTarHeader(tbInfo, header_name, fileName, statbuf) == FALSE) {
		return FALSE;
	}

	/* If it was a regular file, write out the body */
	if (inputFileFd >= 0) {
		size_t readSize;
		/* Write the file to the archive. */
		/* We record size into header first, */
		/* and then write out file. If file shrinks in between, */
		/* tar will be corrupted. So we don't allow for that. */
		/* NB: GNU tar 1.16 warns and pads with zeroes */
		/* or even seeks back and updates header */
		bb_copyfd_exact_size(inputFileFd, tbInfo->tarFd, statbuf->st_size);
		////off_t readSize;
		////readSize = bb_copyfd_size(inputFileFd, tbInfo->tarFd, statbuf->st_size);
		////if (readSize != statbuf->st_size && readSize >= 0) {
		////	bb_error_msg_and_die("short read from %s, aborting", fileName);
		////}

		/* Check that file did not grow in between? */
		/* if (safe_read(inputFileFd, 1) == 1) warn but continue? */

		close(inputFileFd);

		/* Pad the file up to the tar block size */
		/* (a few tricks here in the name of code size) */
		readSize = (-(int)statbuf->st_size) & (TAR_BLOCK_SIZE-1);
		memset(block_buf, 0, readSize);
		xwrite(tbInfo->tarFd, block_buf, readSize);
	}

	return TRUE;
}



/* gcc 4.2.1 inlines it, making code bigger */
static NOINLINE int writeTarFile(int tar_fd, int verboseFlag,
	int recurseFlags, const llist_t *include,
	const llist_t *exclude)
{

	
	int errorFlag = FALSE;
	struct TarBallInfo tbInfo;
	tbInfo.hlInfoHead = NULL;
	tbInfo.tarFd = tar_fd;
	tbInfo.verboseFlag = verboseFlag;

	/* Store the stat info for the tarball's file, so
	 * can avoid including the tarball into itself....  */
	xfstat(tbInfo.tarFd, &tbInfo.tarFileStatBuf, "can't stat tar file");

	tbInfo.excludeList = exclude;

	/* Read the directory/files and iterate over them one at a time */
	while (include) {
		if (!recursive_action(include->data, recurseFlags,
				writeFileToTarball, writeFileToTarball, &tbInfo, 0)
		) {
			errorFlag = TRUE;
		}
		include = include->link;
	}
	/* Write two empty blocks to the end of the archive */
	memset(block_buf, 0, 2*TAR_BLOCK_SIZE);
	xwrite(tbInfo.tarFd, block_buf, 2*TAR_BLOCK_SIZE);

	/* To be pedantically correct, we would check if the tarball
	 * is smaller than 20 tar blocks, and pad it if it was smaller,
	 * but that isn't necessary for GNU tar interoperability, and
	 * so is considered a waste of space */

	/* Close so the child process (if any) will exit */
	close(tbInfo.tarFd);

	/* Hang up the tools, close up shop, head home */
	if (ENABLE_FEATURE_CLEAN_UP)
		freeHardLinkInfo(&tbInfo.hlInfoHead);

	if (errorFlag)
		bb_error_msg("error exit delayed from previous errors");
	return errorFlag;
}






int tarArchive(const char * pathname,const char * tarname)
{
	archive_handle_t *tar_handle;
	tar_handle = init_handle();
	tar_handle->ah_flags = ARCHIVE_CREATE_LEADING_DIRS
	                     | ARCHIVE_RESTORE_DATE
	                     | ARCHIVE_UNLINK_OLD;

	unsigned opt=0;
	int verboseFlag = 0;
	llist_add_to_end(&tar_handle->accept, (void *)pathname);

	{
		
		int tar_fd = STDIN_FILENO;
		int flags = O_RDONLY;
		if (tar_handle->accept == NULL){
			//To do notice
			return -1;
		}

		flags = O_WRONLY | O_CREAT | O_TRUNC;

		if (LONE_DASH(tarname)) {
			tar_handle->src_fd = tar_fd;
			tar_handle->seek = seek_by_read;
		} else {
			tar_handle->src_fd = xopen(tarname, flags);
		}
	}

	
	return writeTarFile(tar_handle->src_fd, verboseFlag,opt|ACTION_RECURSE,
				tar_handle->accept,
				tar_handle->reject);	
}

int tarExtract(const char * tarname)
{
	archive_handle_t *tar_handle;
	tar_handle = init_handle();
	tar_handle->ah_flags = ARCHIVE_CREATE_LEADING_DIRS
	                     | ARCHIVE_RESTORE_DATE
	                     | ARCHIVE_UNLINK_OLD;

	
	int verboseFlag = 0;
	int ret=0;
	if (verboseFlag) tar_handle->action_header = header_verbose_list;
	if (verboseFlag == 1) tar_handle->action_header = header_list;
	tar_handle->action_data = data_extract_all;
	//llist_add_to_end(&tar_handle->accept, (void *)tarname);
	if (tar_handle->accept || tar_handle->reject)
	tar_handle->filter = filter_accept_reject_list;

	/* Open the tar file */
	{
		int tar_fd = STDIN_FILENO;
		int flags = O_RDONLY;

		if (LONE_DASH(tarname)) {
			tar_handle->src_fd = tar_fd;
			tar_handle->seek = seek_by_read;
		} else {
			printf("do this\n");
			tar_handle->src_fd = xopen(tarname, flags);
		}
	}
	ret = EXIT_FAILURE;

	while (get_header_tar(tar_handle) == EXIT_SUCCESS)
		ret = EXIT_SUCCESS; /* saw at least one header, good */

	return ret;

}

int main(int argc,char *argv[])
{
	char *pathname="test";
	char *tarname="test.tar";
	tarArchive(pathname,tarname);

	//tarExtract(tarname);
	return 0;
}







