/* filetest.c: the file services and the C library's files on a drive (tools/sdk). MIT License, as pmimg.h.

     RUN "filetest.app", "A:"        (or "B:", "C:") makes the directory ft_dir on the drive and, in it: writes, reads
                                     back, seeks, appends, truncates, stats, renames, lists and removes files, through
                                     stdio and through the services; writes 1 MB from the PSRAM and reads it back; changes
                                     directory and back; then removes everything. Each check prints PASS or FAIL.
     RUN "filetest.app", "A:", "open"  also leaves a file open at the end, for the loader to close */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "cmm2.h"

static int fails;
static void check(const char *what, int ok) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if(!ok) fails++;
}

int main(int argc, char **argv) {
    const char *drive = argc > 1 ? argv[1] : "A:";
    char dir[64], a[96], b[96], line[128], cwd[300], was[300];
    FILE *f;
    int h, i, n;
    struct stat st;
    cmm2_stat cs;

    snprintf(dir, sizeof(dir), "%s/ft_dir", drive);
    snprintf(a, sizeof(a), "%s/one.txt", dir);
    snprintf(b, sizeof(b), "%s/two.txt", dir);
    remove(a);
    remove(b);
    rmdir(dir);
    check("mkdir", mkdir(dir, 0777) == 0);
    check("stat of the directory", stat(dir, &st) == 0 && S_ISDIR(st.st_mode));

    /* stdio */
    check("fopen w", (f = fopen(a, "w")) != NULL);
    if(f) {
        for(i = 0; i < 100; i++) fprintf(f, "line %d\n", i);
        check("fclose", fclose(f) == 0);
    }
    check("stat size", stat(a, &st) == 0 && st.st_size == 790 && S_ISREG(st.st_mode));
    check("fopen a", (f = fopen(a, "a")) != NULL);
    if(f) { fputs("appended\n", f); fclose(f); }
    check("fopen r", (f = fopen(a, "r")) != NULL);
    if(f) {
        n = 0;
        while(fgets(line, sizeof(line), f)) n++;
        check("101 lines read back", n == 101 && strcmp(line, "appended\n") == 0);
        check("fseek", fseek(f, 7 * 6, SEEK_SET) == 0 && fgets(line, sizeof(line), f) && strcmp(line, "line 6\n") == 0);
        check("ftell", ftell(f) == 7 * 7);
        fclose(f);
    }

    /* the services */
    h = cmm2->file_open(a, CMM2_O_READ | CMM2_O_WRITE);
    check("file_open read/write", h >= 0);
    if(h >= 0) {
        check("file_size", cmm2->file_size(h) == 799);
        check("file_seek, file_write in place", cmm2->file_seek(h, 5) == 0 && cmm2->file_write(h, "X", 1) == 1);
        check("file_seek, file_truncate", cmm2->file_seek(h, 100) == 0 && cmm2->file_truncate(h) == 0 && cmm2->file_size(h) == 100);
        check("file_tell", cmm2->file_tell(h) == 100);
        check("file_sync", cmm2->file_sync(h) == 0);
        check("file_close", cmm2->file_close(h) == 0);
        check("closed handle refused", cmm2->file_close(h) < 0);
    }
    check("file_stat", cmm2->file_stat(a, &cs) == 0 && cs.size == 100 && !cs.is_dir);
    if((f = fopen(a, "r")) != NULL) {
        check("written in place", fgets(line, sizeof(line), f) && strcmp(line, "line X\n") == 0);
        fclose(f);
    }
    check("rename", rename(a, b) == 0 && stat(a, &st) != 0 && stat(b, &st) == 0);
    check("no such file: FR_NO_FILE", cmm2->file_open(a, CMM2_O_READ) == -4);

    /* 1 MB from the PSRAM to the drive and back (A:: a flash write turns the PSRAM off meanwhile) */
    {
        uint32_t sz = 1 << 20, t0;
        uint8_t *m = malloc(sz), *r = malloc(sz);
        check("malloc 2 MB", m && r);
        if(m && r) {
            for(i = 0; i < (int)sz; i++) m[i] = (uint8_t)(i * 7 + (i >> 9));
            snprintf(line, sizeof(line), "%s/big.bin", dir);
            t0 = (uint32_t)cmm2->micros();
            h = cmm2->file_open(line, CMM2_O_WRITE | CMM2_O_CREATE);
            n = h >= 0 ? cmm2->file_write(h, m, sz) : h;
            if(h >= 0) cmm2->file_close(h);
            printf("  1 MB written in %lu ms\n", (unsigned long)(((uint32_t)cmm2->micros() - t0) / 1000));
            check("1 MB written", n == (int)sz);
            t0 = (uint32_t)cmm2->micros();
            h = cmm2->file_open(line, CMM2_O_READ);
            n = h >= 0 ? cmm2->file_read(h, r, sz) : h;
            if(h >= 0) cmm2->file_close(h);
            printf("  1 MB read in %lu ms\n", (unsigned long)(((uint32_t)cmm2->micros() - t0) / 1000));
            check("1 MB read back the same", n == (int)sz && memcmp(m, r, sz) == 0);
            check("big.bin removed", remove(line) == 0);
        }
        free(m);
        free(r);
    }

    /* listing, the current directory */
    {
        DIR *d = opendir(dir);
        int seen = 0, dots = 0;
        struct dirent *e;
        check("opendir", d != NULL);
        if(d) {
            while((e = readdir(d)) != NULL) {
                printf("  %s%s\n", e->d_name, e->d_type == DT_DIR ? "/" : "");
                if(strcmp(e->d_name, "two.txt") == 0) seen = 1;
                if(e->d_name[0] == '.') dots++;
            }
            closedir(d);
        }
        check("readdir found two.txt, . and ..", seen && dots == 2);
    }
    check("getcwd", getcwd(was, sizeof(was)) != NULL);
    printf("  was %s\n", was);
    check("chdir", chdir(dir) == 0 && getcwd(cwd, sizeof(cwd)) != NULL);
    printf("  now %s\n", cwd);
    check("a relative name", (f = fopen("two.txt", "r")) != NULL);
    if(f) fclose(f);
    snprintf(line, sizeof(line), "%s/", drive);                     /* (each drive has its own current directory, and
                                                                       one that is current cannot be removed) */
    check("chdir to the root and back", chdir(line) == 0 && chdir(was) == 0 && getcwd(cwd, sizeof(cwd)) && strcmp(cwd, was) == 0);

    if(argc > 2 && strcmp(argv[2], "open") == 0) {
        h = cmm2->file_open(b, CMM2_O_READ);
        printf("  left open: handle %d\n", h);
        printf("%d failed\n", fails);
        return fails;
    }
    check("remove", remove(b) == 0);
    check("rmdir", rmdir(dir) == 0 && stat(dir, &st) != 0);
    printf("%d failed\n", fails);
    return fails;
}
