#include <fcntl.h>
#include <linux/fs.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/ioctl.h>

uint64_t get_blkdev_size(char *path);