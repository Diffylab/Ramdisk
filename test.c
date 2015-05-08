 /*
   -- template test file for RAMDISK Filesystem Assignment.
   -- include a case for:
   -- two processes
   -- largest number of files (should be 1024 max)
   -- largest single file (start with direct blocks [2048 bytes max],
   then single-indirect [18432 bytes max] and finally double
   indirect [1067008 bytes max])
   -- creating and unlinking files to avoid memory leaks
   -- each file operation
   -- error checking on invalid inputs
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include "api.h"

// #define's to control what tests are performed,
// comment out a test if you do not wish to perform it

#define TEST1
#define TEST2
#define TEST3
#define TEST4
#define TEST5


#define USE_RAMDISK

#ifdef USE_RAMDISK
#define CREAT   rd_creat
#define OPEN    rd_open
#define WRITE   rd_write
#define READ    rd_read
#define UNLINK  rd_unlink
#define MKDIR   rd_mkdir
#define READDIR rd_readdir
#define CLOSE   rd_close
#define LSEEK   rd_lseek

#else
#define CREAT(file)   creat(file, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define OPEN(file)    open(file, O_RDWR)
#define WRITE   write
#define READ    read
#define UNLINK  unlink
#define MKDIR(path)   mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
#define READDIR my_readdir
#define CLOSE   close
#define LSEEK(fd, offset)   lseek(fd, offset, SEEK_SET)

int my_readdir(int fd, char* str)
{
  strcpy(str, "blah");
  return 1;
}

#endif


// #define's to control whether single indirect or
// double indirect block pointers are tested

#define TEST_SINGLE_INDIRECT
#define TEST_DOUBLE_INDIRECT


#define MAX_FILES 1023
#define BLK_SZ 256		/* Block size */
#define DIRECT 8		/* Direct pointers in location attribute */
#define PTR_SZ 4		/* 32-bit [relative] addressing */
#define PTRS_PB  (BLK_SZ / PTR_SZ) /* Pointers per index block */

static char pathname[80];

static char data1[DIRECT*BLK_SZ]; /* Largest data directly accessible */
static char data2[PTRS_PB*BLK_SZ];     /* Single indirect data size */
static char data3[PTRS_PB*PTRS_PB*BLK_SZ]; /* Double indirect data size */
static char addr[PTRS_PB*PTRS_PB*BLK_SZ+1]; /* Scratchpad memory */

int main () {

  int retval, i;
  int fd;
  int index_node_number;

  /* Some arbitrary data for our files */
  memset (data1, '1', sizeof (data1));
  memset (data2, '2', sizeof (data1));
  memset (data3, '3', sizeof (data1));


#ifdef TEST1

  /* ****TEST 1: MAXIMUM file creation**** */

  /* Generate MAXIMUM regular files */
  for (i = 0; i < MAX_FILES + 1; i++) { // go beyond the limit
    sprintf (pathname, "/file%d", i);

    retval = CREAT (pathname);

    if (retval < 0) {
      fprintf (stderr, "creat: File creation error! status: %d\n",
	       retval);

      if (i != MAX_FILES)
	exit(EXIT_FAILURE);
    }

    memset (pathname, 0, 80);
  }

  /* Delete all the files created */
  for (i = 0; i < MAX_FILES; i++) {
    sprintf (pathname, "/file%d", i);

    retval = UNLINK (pathname);

    if (retval < 0) {
      fprintf (stderr, "unlink: File deletion error! status: %d\n",
	       retval);

      exit(EXIT_FAILURE);
    }

    memset (pathname, 0, 80);
  }

#endif // TEST1

#ifdef TEST2

  /* ****TEST 2: LARGEST file size**** */


  /* Generate one LARGEST file */
  retval = CREAT ("/bigfile");

  if (retval < 0) {
    fprintf (stderr, "creat: File creation error! status: %d\n",
	     retval);

    exit(EXIT_FAILURE);
  }

  retval =  OPEN ("/bigfile"); /* Open file to write to it */

  if (retval < 0) {
    fprintf (stderr, "open: File open error! status: %d\n",
	     retval);

    exit(EXIT_FAILURE);
  }

  fd = retval;			/* Assign valid fd */

  /* Try writing to all direct data blocks */
  retval = WRITE (fd, data1, sizeof(data1));

  if (retval < 0) {
    fprintf (stderr, "write: File write STAGE1 error! status: %d\n",
	     retval);

    exit(EXIT_FAILURE);
  }

#ifdef TEST_SINGLE_INDIRECT

  /* Try writing to all single-indirect data blocks */
  retval = WRITE (fd, data2, sizeof(data2));

  if (retval < 0) {
    fprintf (stderr, "write: File write STAGE2 error! status: %d\n",
	     retval);

    exit(EXIT_FAILURE);
  }

#ifdef TEST_DOUBLE_INDIRECT

  /* Try writing to all double-indirect data blocks */
  retval = WRITE (fd, data3, sizeof(data3));

  if (retval < 0) {
    fprintf (stderr, "write: File write STAGE3 error! status: %d\n",
	     retval);

    exit(EXIT_FAILURE);
  }

#endif // TEST_DOUBLE_INDIRECT

#endif // TEST_SINGLE_INDIRECT

#endif // TEST2

#ifdef TEST3

  /* ****TEST 3: Seek and Read file test**** */
  retval = LSEEK (fd, 0);	/* Go back to the beginning of your file */

  if (retval < 0) {
    fprintf (stderr, "lseek: File seek error! status: %d\n",
	     retval);

    exit(EXIT_FAILURE);
  }

  /* Try reading from all direct data blocks */
  retval = READ (fd, addr, sizeof(data1));

  if (retval < 0) {
    fprintf (stderr, "read: File read STAGE1 error! status: %d\n",
	     retval);

    exit(EXIT_FAILURE);
  }
  /* Should be all 1s here... */
  printf ("Data at addr: %s\n", addr);

#ifdef TEST_SINGLE_INDIRECT

  /* Try reading from all single-indirect data blocks */
  retval = READ (fd, addr, sizeof(data2));

  if (retval < 0) {
    fprintf (stderr, "read: File read STAGE2 error! status: %d\n",
	     retval);

    exit(EXIT_FAILURE);
  }
  /* Should be all 2s here... */
  printf ("Data at addr: %s\n", addr);

#ifdef TEST_DOUBLE_INDIRECT

  /* Try reading from all double-indirect data blocks */
  retval = READ (fd, addr, sizeof(data3));

  if (retval < 0) {
    fprintf (stderr, "read: File read STAGE3 error! status: %d\n",
	     retval);

    exit(EXIT_FAILURE);
  }
  /* Should be all 3s here... */
  printf ("Data at addr: %s\n", addr);

#endif // TEST_DOUBLE_INDIRECT

#endif // TEST_SINGLE_INDIRECT

  /* Close the bigfile */
  retval = CLOSE(fd);

  if (retval < 0) {
    fprintf (stderr, "close: File close error! status: %d\n",
	     retval);

    exit(EXIT_FAILURE);
  }

  /* Remove the biggest file */

  retval = UNLINK ("/bigfile");

  if (retval < 0) {
    fprintf (stderr, "unlink: /bigfile file deletion error! status: %d\n",
	     retval);

    exit(EXIT_FAILURE);
  }

#endif // TEST3

#ifdef TEST4

  /* ****TEST 4: Make directory and read directory entries**** */
  retval = MKDIR ("/dir1");

  if (retval < 0) {
    fprintf (stderr, "mkdir: Directory 1 creation error! status: %d\n",
	     retval);

    exit(EXIT_FAILURE);
  }

  retval = MKDIR ("/dir1/dir2");

  if (retval < 0) {
    fprintf (stderr, "mkdir: Directory 2 creation error! status: %d\n",
	     retval);

    exit(EXIT_FAILURE);
  }

  retval = MKDIR ("/dir1/dir3");

  if (retval < 0) {
    fprintf (stderr, "mkdir: Directory 3 creation error! status: %d\n",
	     retval);

    exit(EXIT_FAILURE);
  }

  retval =  OPEN ("/dir1"); /* Open directory file to read its entries */

  if (retval < 0) {
    fprintf (stderr, "open: Directory open error! status: %d\n",
	     retval);

    exit(EXIT_FAILURE);
  }

  fd = retval;			/* Assign valid fd */

  memset (addr, 0, sizeof(addr)); /* Clear scratchpad memory */

  while ((retval = READDIR (fd, addr))) { /* 0 indicates end-of-file */

    if (retval < 0) {
      fprintf (stderr, "readdir: Directory read error! status: %d\n",
	       retval);
      exit(EXIT_FAILURE);
    }

    index_node_number = atoi(&addr[14]);
    printf ("Contents at addr: [%s,%d]\n", addr, index_node_number);
  }

#endif // TEST4

#ifdef TEST5

  /* ****TEST 5: 2 process test**** */

  if((retval = fork())) {

    if(retval == -1) {
      fprintf(stderr, "Failed to fork\n");
      exit(EXIT_FAILURE);
    }

    /* Generate 300 regular files */
    for (i = 0; i < 300; i++) {
      sprintf (pathname, "/file_p_%d", i);

      retval = CREAT (pathname);

      if (retval < 0) {
	fprintf (stderr, "(Parent) create: File creation error! status: %d\n",
		 retval);
	exit(EXIT_FAILURE);
      }

      memset (pathname, 0, 80);
    }

  }
  else {
    /* Generate 300 regular files */
    for (i = 0; i < 300; i++) {
      sprintf (pathname, "/file_c_%d", i);

      retval = CREAT (pathname);

      if (retval < 0) {
	fprintf (stderr, "(Child) create: File creation error! status: %d\n",
		 retval);

	exit(EXIT_FAILURE);
      }

      memset (pathname, 0, 80);
    }
  }

#endif // TEST5

  printf("Congratulations, you have passed all tests!!\n");

  return 0;
}

