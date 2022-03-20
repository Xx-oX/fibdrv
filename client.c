#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

/*
 * Set the method for calculation fibonacci sequence
 */
#define ORIGINAL 0
#define FAST_DOUBLING 1

#define FIB_METHOD FAST_DOUBLING

#if FIB_METHOD == ORIGINAL
#define PATH "./plot/original"
#elif FIB_METHOD == FAST_DOUBLING
#define PATH "./plot/fast_doubling"
#endif


int main()
{
    struct timespec ts_start, ts_end;

    long long sz;

    char buf[1];
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    FILE *fp = fopen(PATH, "w+");
    if (!fp) {
        printf("Time record error!\n");
    }

    for (int i = 0; i <= offset; i++) {
        clock_gettime(CLOCK_REALTIME, &ts_start);
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        clock_gettime(CLOCK_REALTIME, &ts_end);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);

        char tbuf[16];
        snprintf(tbuf, 16, "%d %ld\n", i, ts_end.tv_nsec - ts_start.tv_nsec);
        fwrite(tbuf, sizeof(char), strlen(tbuf), fp);
    }
    fclose(fp);

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }

    close(fd);
    return 0;
}
