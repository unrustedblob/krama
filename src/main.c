// Read Krama source file and dump to stdout

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
        FILE *fp = fopen("/home/unrust/projects/krama/tests/krama_src/test.krm", "rb");
        int exit_status = 1;

        if (!fp) {
                fprintf(stderr, "Unable to open file!\n%s\n", strerror(errno));
                return exit_status;
        }

        if (fseek(fp, 0, SEEK_END)) {
                fprintf(stderr, "Seek error!\n%s\n", strerror(errno));
                goto cleanup_file;
        }

        long end_pos = ftell(fp);

        if (end_pos < 0) {
                fprintf(stderr, "Unable to obtain file size!\n%s\n", strerror(errno));
                goto cleanup_file;
        }

        rewind(fp);

        size_t file_sz = (size_t)end_pos; // Checked non-negative above
        char *buf = malloc(file_sz + 1);

        if (!buf) {
                fprintf(stderr, "Allocation failed. Unable to alloc %zu bytes.", file_sz);
                goto cleanup_file;
        }
        buf[file_sz] = '\0';

        size_t read_sz = fread(buf, 1, file_sz, fp);

        if (read_sz < file_sz) {
                if (ferror(fp)) {
                        fprintf(stderr, "Error occured while reading file!\n%s\n", strerror(errno));
                } else if (feof(fp)) {
                        fprintf(stderr, "Unexpected EOF encountered!\nExpected %zu bytes got %zu\n",
                                file_sz, read_sz);
                }
                goto cleanup_alloc;
        }

        printf("%s\n", buf);
        exit_status = 0;

cleanup_alloc:
        free(buf);

cleanup_file:
        fclose(fp);

        return exit_status;
}
