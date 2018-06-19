// Playing around with measure data
// generated by https://github.com/FOULAB/Project-COGSWORTH/

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "colormaps.h"

RGBFloatCol *const colormap = kPlasmaColors;  // see colormaps.h for choice

// Y between even/odd is shifted apparently due to the mechanics.
// Some empirical fudge-value
int kDataShift = 20;

static int usage(const char *prog) {
    fprintf(stderr, "Usage: %s <filename-pattern> <width> <height>\n", prog);
    fprintf(stderr, "Output is written to stdout\n");
    fprintf(stderr, "Example:\n"
            "%s SAMPLE_3162099_%%03d-%%03d.dmp 200 70 > foo.pnm\n",
            prog);
    return 1;
}

// Don't include bogus numbers.
static bool looks_reasonable(float v) { return v >= 0 && v <= 1000; }

int main(int argc, char *argv[]) {
    if (argc != 4) return usage(argv[0]);
    const char *pattern = argv[1];
    const int w = atoi(argv[2]);
    const int h = atoi(argv[3]);
    int out_fd = STDOUT_FILENO;

    const int img_w = w;
    const int img_h = h + kDataShift;

    // Let's first determine the range of values, before we flatten it
    // to a 256 image
    float min_value = 1e6, max_value = -1e6;
    float raw_image[img_w * img_h] = {0};
    int64_t total_data_read = 0;

    char filename[256];
    struct stat statbuf;
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) {
            snprintf(filename, sizeof(filename), pattern, x, y);
            const int fd = open(filename, O_RDONLY);

            // Little bit of sanity checking.
            if (fd < 0 ||
                fstat(fd, &statbuf) < 0 ||
                statbuf.st_size % sizeof(float) != 0) {
                fprintf(stderr, "%s: %s\n", filename, strerror(errno));
                if (fd > 0) close(fd);
                continue;
            }

            fprintf(stderr, "\b\b\b\b\b\b\b%03d %03d", x, y);
            float *values = (float*) mmap(NULL, statbuf.st_size, PROT_READ,
                                          MAP_SHARED, fd, 0);
            if (values == MAP_FAILED) {
                fprintf(stderr, "mmap %s; %s\n", filename, strerror(errno));
                continue;
            }
            // Let's determine the average of the measurement at a 'pixel'
            float sum = 0.0;
            int count = statbuf.st_size / sizeof(float);
            for (int i = 0; i < count; ++i)
                if (looks_reasonable(values[i])) sum += values[i];
            total_data_read += statbuf.st_size;

            munmap(values, statbuf.st_size);
            close(fd);

            // Assign pixels with a little bit of fudging because of the
            // kDataShift. In regions where we only have even or odd data
            // available, we just fill in the adjacent pixel. That creates lower
            // picture at the top and bottom, but better than nothing.
            int ypos = x % 2 == 0 ? y : img_h - y - 1; // Scanning goes up/down
            int xpos = img_w - x - 1;                  // scanning right2left
            float avg = sum / count;
            if (x % 2 == 0 && y < kDataShift) {
                // Here, we don't have the adjacent data; just fill next pixel
                // as well.
                raw_image[ypos * w + xpos] = avg;
                raw_image[ypos * w + xpos-1] = avg;
            } else if (x % 2 == 1 && ypos >= h) {
                raw_image[ypos * w + xpos] = avg;
                raw_image[ypos * w + xpos+1] = avg;
            } else {
                // Regular pixel
                raw_image[ypos * w + xpos] = avg;
            }
            // TODO: record a histogram, so that we can scale things in the
            // 2..98 percentile.
            if (avg < min_value) min_value = avg;
            if (avg > max_value) max_value = avg;
        }
    }

    fprintf(stderr, "\nSeen range from %.3f .. %.3f. Processed %.2f Mbytes\n",
            min_value, max_value, total_data_read / 1024.0 / 1024.0);

    // Dump out the PNM image.
    dprintf(out_fd, "P6\n%d %d\n255\n", img_w, img_h);
    const float scale = 255 / (max_value - min_value);
    for (int y = 0; y < img_h; ++y) {
        for (int x = 0; x < img_w; ++x) {
            float raw = raw_image[y * w + x];
            int val = (raw < min_value) ? 0 : round((raw - min_value) * scale);
            if (val < 0) val = 0;  // Put into range in case we used histogram
            if (val > 255) val = 255;
            const RGBFloatCol &c = colormap[val];
            unsigned char col_bits[3] = { (unsigned char) lround(c.r * 255),
                                          (unsigned char) lround(c.g * 255),
                                          (unsigned char) lround(c.b * 255)};
            write(out_fd, col_bits, 3);
        }
    }
}
