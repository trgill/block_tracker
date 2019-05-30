#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

extern int errno;

uint64_t get_blkdev_size(const char *path) {
  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "%s", strerror(errno));
  }

  uint64_t size;
  if (ioctl(fd, BLKGETSIZE64, &size) == -1) {
    fprintf(stderr, "%s", strerror(errno));
  }
  close(fd);

  return size;
}
