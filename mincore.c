#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif /* _XOPEN_SOURCE */

#ifdef __linux
#define _DEFAULT_SOURCE
#endif /* __linux */

#include <stdio.h>
#include <stdlib.h>
#define __STDC_LIMIT_MACROS 1
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#include <unistd.h>
#include <sys/mman.h>

#ifdef __sun
#define VECTYPE char
#else /* __sun */
#define VECTYPE unsigned char
#endif /* __sun */

#ifdef DEBUG
#define DEBUG1(c) c
#else /* DEBUG */
#define DEBUG1(c)
#endif /* DEBUG */

#define APP_NAME "mincore"
#define VERSION "0.9"

struct pagecorestat {
  int in;
  int out;
  int err;
  char msg[256];
};

struct pagecorestat checkcorestat(char *file);

/*
 *  usage()
 *	simple help
 */
static void usage(char *const argv[])
{
	(void)printf("%s, version %s\n\n", APP_NAME, VERSION);
	(void)printf("usage: %s [-h] file ...\n", argv[0]);
	(void)printf("-h\tshow usage.\n");
}


int main(int argc, char *argv[]) {
  int f;
  long pagesize = sysconf(_SC_PAGESIZE); /* bytes per page */
  double mb_per_page = (double)pagesize / (1024.0 * 1024.0); /* MB per page */
  struct pagecorestat pagestat;
  int tpi = 0, tpo = 0; /* summation counters for pages in/out */

  if ( argc < 2 ) {
    usage(argv);
    exit(EXIT_FAILURE);
  }
  for (;;) {
    const int c = getopt(argc, argv, "h");
    if (c == -1)
      break;
    switch (c) {
    case 'h':
      usage(argv);
      exit(EXIT_SUCCESS);
    default:
      usage(argv);
      exit(EXIT_FAILURE);
    }
  }
  
  if (pagesize == 0L) {
    (void)fprintf(stderr, "ERROR: page size is %ld\n", pagesize);
    exit(EXIT_FAILURE);
  }

  (void)fprintf(stdout, "%s",
                "  pages     pages not  %\n"
                " in core     in core   in          file\n"
                "---------- ---------- ----  --------------------\n");

  for (f = 1; f < argc; f++) {
    if ((pagestat = checkcorestat(argv[f])).err) {
      fprintf(stdout, "%10s %10s %3s   %s: %s\n", "", "", "-", argv[f],
              pagestat.msg);
    } else {
      (void)fprintf(stdout, "%10d %10d %3d   %s\n", pagestat.in, pagestat.out,
              (int)100. * pagestat.in / (pagestat.in + pagestat.out), argv[f]);
      tpi += pagestat.in;
      tpo += pagestat.out;
    }
  }
  (void)fprintf(stdout, "%s%10d %10d %3d   total pages\n",
                "---------- ---------- ----  --------------------\n", tpi, tpo,
                (int)((tpi + tpo) > 0 ? 100. * tpi / (tpi + tpo) : 0));
  (void)fprintf(stdout, "%10.1f %10.1f       total MB (%ldkb pages)\n",
                tpi * mb_per_page, tpo * mb_per_page, (pagesize / 1024));

  exit(EXIT_SUCCESS);
  /* quiet lint and lclint: */
  /* NOTREACHED */
  /*@-unreachable@*/
  return (0);
}

struct pagecorestat checkcorestat(char *file) {
  int i, fd, errnum;
  void *ptr;
  struct stat st;
  struct pagecorestat ps = {0, 0, 0, ""};
  long pagesize = sysconf(_SC_PAGESIZE); /* bytes per page */
  VECTYPE *vec, *vecseg;
  size_t veclen;
  size_t mapsize, maxmapsize = SIZE_MAX, minmapsize = 1024 * pagesize;

  if (stat(file, &st) == -1) {
    ps.err++;
    strncpy(ps.msg, strerror(errno), 255);
    return ps;
  }
  if (!S_ISREG(st.st_mode)) {
    strncpy(ps.msg, "not a regular file", 255);
    ps.err++;
    return ps;
  }
  if (st.st_size == 0) {
    strncpy(ps.msg, "zero length", 255);
    ps.err++;
    return ps;
  }
  /* how many pages does the file have? Need a vec with this many chars */
  veclen = (st.st_size / pagesize) + ((st.st_size % pagesize) != 0 ? 1 : 0);

  if ((fd = open(file, O_RDONLY)) < 0) {
    errnum = errno;
    strncpy(ps.msg, strerror(errnum), 255);
    ps.err++;
    return ps;
  }

  DEBUG1((void)fprintf(stderr,
                       "file size is %lld, page size is %ld, veclen is %ld\n",
                       (long long)st.st_size, pagesize, veclen);)

  if (!(vec = malloc(veclen))) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  assert(st.st_size <= SIZE_MAX);
#ifdef DEBUG
  if (st.st_size > SIZE_MAX) {
    DEBUG1((void)fprintf(stderr, "%lu is greater than SIZE_MAX (%ld).\n",
                         (unsigned long)st.st_size, SIZE_MAX););
  }
#endif
  vecseg = vec;
  while (st.st_size > 0) {
    mapsize = st.st_size > maxmapsize ? maxmapsize : st.st_size;
    if ( (ptr = mmap(/*@null@*/ 0, mapsize, PROT_READ, MAP_PRIVATE, fd, 0) ) ==
        ((void *)-1)) {

      if (errno == ENOMEM && (mapsize > 2 * minmapsize)) {
        /* if we failed to mmap due to lack of memory, halve the mapsize and
         * let it try again */
        maxmapsize /= 2;
        DEBUG1(
            (void)fprintf(stderr,
                          "mmap returned ENOMEM, halving maxmapsize to %ldkb\n",
                          (long)maxmapsize / 1024);)
        continue;
      } else {
        perror("mmap");
        exit(EXIT_FAILURE);
      }
    }

    if (mincore(ptr, mapsize, vecseg)) {
      perror("mincore");
      exit(EXIT_FAILURE);
    }
    if (munmap(ptr, mapsize) == -1) {
      perror("munmap");
      exit(EXIT_FAILURE);
    }
    st.st_size -= mapsize;
    vecseg += mapsize / pagesize;
  }
  if (close(fd)) {
    perror("close");
  }

  for (i = 0; i < veclen; i++) {
    /* fprintf(stderr,"vec[%d] = 0x%2x\n",i,vec[i]); */
    if (vec[i] & 0x01) {
      ps.in++;
    } else {
      ps.out++;
    }
  }
  DEBUG1((void)fprintf(stderr, "file %s pi: %d  po: %d\n", argv[f], ps.in,
                       ps.out);)
  free(vec);

  return ps;
}
