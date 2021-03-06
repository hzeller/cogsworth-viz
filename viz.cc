// -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
// Playing around with measure data
// generated by https://github.com/FOULAB/Project-COGSWORTH/

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "colormaps.h"

static const RGBCol *const color_maps[] = {
    kGreyColors,
    kPlasmaColors,
    kMagmaColors,
    kInfernoColors,
    kViridisColors,
};

// Y between even/odd is shifted apparently due to the mechanics.
// Some empirical fudge-value
int kDefaultDataShift = 10;

static int exit_with_msg(const char *msg) {
    fprintf(stderr, "%s%s\n\n", msg, errno != 0 ? strerror(errno) : "");
    return 2;
}

static int usage(const char *prog) {
    fprintf(stderr, "Usage: %s [options] <filename-pattern> <width> <height>\n",
            prog);
    fprintf(stderr, "\t -c <color>  : Color map. One of [0..4]. Default 1.\n"
            "\t               (0=Grey 1=Plasma 2=Magma 3=Inferno 4=Viridis)\n");
    fprintf(stderr, "\t -s <shift>  : Data shift going up/down; "
            "fix 'comb-effect'. Default 10.\n");
    fprintf(stderr, "\t -o <outfile>: Output filename for PNM image. "
            "Default stdout\n");
    fprintf(stderr, "Example:\n"
            "%s -c3 -o foo.pnm SAMPLE_3162099_%%d_%%d.dmp 207 80\n", prog);
    return 1;
}

// Don't include bogus numbers.
static bool looks_reasonable(float v) { return v >= 0 && v <= 1000; }

int main(int argc, char *argv[]) {
    errno = 0;
    int data_shift = kDefaultDataShift;
    unsigned int color_map_index = 1;
    int out_fd = STDOUT_FILENO;
    int opt;
    while ((opt = getopt(argc, argv, "c:o:s:")) != -1) {
        switch (opt) {
        case 's': data_shift = atoi(optarg); break;
        case 'c': color_map_index = atoi(optarg); break;
        case 'o': out_fd = open(optarg, O_WRONLY|O_TRUNC|O_CREAT, 0644); break;
        }
    }

    if (optind + 3 !=  argc) return usage(argv[0]);
    const char *pattern = argv[optind];
    const int w = atoi(argv[optind+1]);
    const int h = atoi(argv[optind+2]);

    if (out_fd < 0) return exit_with_msg("Cannot open output file.");
    if (color_map_index > 4) return exit_with_msg("Invalid color map.");
    if (data_shift < 0) return exit_with_msg("Only positive data shift.");

    const RGBCol *const colormap = color_maps[color_map_index];
    const int img_w = w;
    const int img_h = h + data_shift;

    // Let's first determine the range of values, before we flatten it
    // to a 256 color image
    float min_value = 1e6, max_value = -1e6;
    float *raw_image = new float[img_w * img_h];
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

            fprintf(stderr, "\b\b\b\b\b\b\b%03d %03d", x, y); // show progress
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

            // Assign pixels with little bit of fudging because of the
            // data shift. In regions where we only have even or odd data
            // available, we just fill in the adjacent pixel. That creates
            // lower-res picture at the top and bottom, but better than nothing.
            int ypos = x % 2 == 0 ? y : img_h - y - 1; // Scanning goes up/down
            int xpos = img_w - x - 1;                  // scanning right2left
            float avg = sum / count;
            if (x % 2 == 0 && y < data_shift) {
                raw_image[ypos * w + xpos] = raw_image[ypos * w + xpos+1] = avg;
            } else if (x % 2 == 1 && ypos >= h) {
                raw_image[ypos * w + xpos] = raw_image[ypos * w + xpos-1] = avg;
            } else {
                raw_image[ypos * w + xpos] = avg;  // Regular pixel.
            }

            // TODO: record a histogram, so that we can scale things
            // e.g. in the 2..98 percentile.
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
            int val = (raw < min_value) ? 0 : lround((raw - min_value) * scale);
            if (val < 0) val = 0;  // Put into range in case we used histogram
            if (val > 255) val = 255;
            write(out_fd, colormap[val], 3);
        }
    }
    close(out_fd);
    delete [] raw_image;
}
