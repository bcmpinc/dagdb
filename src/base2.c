
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
	int fd = open(database, O_RDWR | O_CREAT, 0644);
	if (fd == -1) goto error;
	file = mmap(NULL, 1<<24, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	ftruncate(fd,1024); // TODO: Remove here, use for growing file size.
	if (file == MAP_FAILED) goto error;
	printf("%p\n", file);
	database_fd = fd;
	((int*)file)[0] = 1; // TODO: Remove, 
	printf("Opened DB\n");
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
	if (file) {
		munmap(file, 1<<24);
		file = 0;
	}
	if (database_fd) {
		close(database_fd);
		database_fd = 0;
		printf("Closed DB\n");
	}
}

