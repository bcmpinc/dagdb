
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include "base2.h"

static int database_fd;
static void * file;

/**
 * Opens the given file.
 * @returns -1 on failure.
 */
int dagdb_load(const char* database)
{
	int fd = open(database, O_RDWR | O_CREAT, 0x644);
	if (fd == -1) goto error;
	file = mmap(NULL, 1<<24, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (file == MAP_FAILED) goto error;
	database_fd = fd;
	return 0;

error:
	perror("dagdb_load");
	if (file != MAP_FAILED) file = 0;
	if (fd != -1) close(fd);
	return -1;
}

/**
 * Closes the current database file. 
 * Does nothing if no database file is currently open.
 */
void dagdb_unload()
{
	if (database_fd) {
		close(database_fd);
		database_fd = 0;
	}
}

