// Read Krama source file and dump to stdout

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "exit_codes.h"

int main(int argc, char *argv[])
{
        int exit_status = KRM_EXIT_ERR_UNKNOWN;
        const char *path = nullptr;

        if (argc == 2) {
                path = argv[1];
        } else {
                fprintf(stderr, "Usage error: %s\n",
                        (argc < 2) ? "No file provided" : "Too many arguments");
                return KRM_EXIT_ERR_USAGE;
        }

        FILE *fp = fopen(path, "rb");
        if (!fp) {
                fprintf(stderr, "Unable to open file!\n%s\n", strerror(errno));
                return KRM_EXIT_ERR_INPUT;
        }

        struct stat stat_buf;
        if (fstat(fileno(fp), &stat_buf)) {
                fprintf(stderr, "Unable to stat file: %s\n", strerror(errno));
                exit_status = KRM_EXIT_ERR_ENV;
                goto cleanup_file;
        }
        if (!S_ISREG(stat_buf.st_mode)) {
                fprintf(stderr, "Not a regular file: %s\n", path);
                exit_status = KRM_EXIT_ERR_INPUT;
                goto cleanup_file;
        }

        size_t file_sz = (size_t)stat_buf.st_size;
        char *buf = malloc(file_sz + 1);
        if (!buf) {
                fprintf(stderr, "Allocation failed. Unable to alloc %zu bytes.", file_sz);
                exit_status = KRM_EXIT_ERR_OOM;
                goto cleanup_file;
        }
        buf[file_sz] = '\0';

        size_t read_sz = fread(buf, 1, file_sz, fp);
        if (read_sz < file_sz) {
                if (ferror(fp)) {
                        fprintf(stderr, "Error occured while reading file!\n%s\n", strerror(errno));
                        exit_status = KRM_EXIT_ERR_ENV;
                } else if (feof(fp)) {
                        fprintf(stderr, "Unexpected EOF encountered!\nExpected %zu bytes got %zu\n",
                                file_sz, read_sz);
                        exit_status = KRM_EXIT_ERR_ENV;
                }
                goto cleanup_alloc;
        }
        printf("%s\n", buf);
        exit_status = KRM_EXIT_SUCCESS;

cleanup_alloc:
        free(buf);

cleanup_file:
        fclose(fp);
        return exit_status;
}
