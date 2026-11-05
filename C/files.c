/* files.c - fopen modes, stat, rename, opendir/readdir and the errors of
   remove/rmdir, on the current drive (run it on A: and on B:).
   Each step says what it did; "FAIL" marks what did not work. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <picomite/basic.h>
#include <picomite/files.h>

static int fails;

static void fail(const char *what)
{
    printf("FAIL %s: %s\n", what, pm_errmsg());
    fails++;
}

/* fopen name in mode, write s if it is not NULL; 0 if all went well */
static int put(const char *name, const char *mode, const char *s)
{
    FILE *f = fopen(name, mode);
    if (f == NULL)
        return fail(mode), -1;
    if (s && fputs(s, f) < 0)
        fail("fputs");
    if (fclose(f))
        fail("fclose");
    return 0;
}

int main(void)
{
    char cwd[128], name[64];
    struct stat st;
    DIR *d;
    struct dirent *e;
    FILE *f;

    if (getcwd(cwd, sizeof cwd) == NULL)
        fail("getcwd");
    printf("cwd %s, drive %s, free %lld\n", cwd, pm_drive_name(name, sizeof name), pm_free_space());
    if (mkdir("ctest", 0) && pm_exists_dir("ctest") != 1)
        return fail("mkdir"), 1;

    put("ctest/log.txt", "w", "one\n");
    put("ctest/log.txt", "a", "two\n");
    if ((f = fopen("ctest/log.txt", "r+")) == NULL)
        fail("r+");
    else
    {
        fputs("ONE", f); /* over "one" */
        fseek(f, 0, SEEK_SET);
        if (fgets(name, sizeof name, f))
            printf("r+: %s", name);
        fclose(f);
    }
    if ((f = fopen("ctest/log.txt", "a+")) == NULL)
        fail("a+");
    else
    {
        fputs("three\n", f); /* at the end, whatever the position */
        rewind(f);
        while (fgets(name, sizeof name, f))
            printf("a+: %s", name); /* ONE two three */
        fclose(f);
    }
    if ((f = fopen("ctest/none.txt", "r")) != NULL)
        printf("FAIL r of a missing file\n"), fails++, fclose(f);

    if (stat("ctest/log.txt", &st) == 0)
        printf("stat: %ld bytes, %s, mtime %lld\n", st.st_size, S_ISREG(st.st_mode) ? "file" : "dir",
               (long long)st.st_mtime);
    else
        fail("stat");
    if (rename("ctest/log.txt", "ctest/log2.txt"))
        fail("rename");

    if ((d = opendir("ctest")) == NULL)
        fail("opendir");
    else
    {
        char first[64];
        pm_dir("*", PM_DIR_ALL, first, sizeof first); /* DIR$ meanwhile: no matter */
        while ((e = readdir(d)) != NULL)
            printf("  %s%s\n", e->d_name, e->d_type == DT_DIR ? "/" : "");
        closedir(d);
    }

    if (remove("nonexistent.txt") == 0)
        printf("FAIL remove of a missing file\n"), fails++;
    else
        printf("remove of a missing file: %s (as expected)\n", pm_errmsg());
    if (remove("ctest/log2.txt"))
        fail("remove");
    if (rmdir("ctest"))
        fail("rmdir");
    printf("ctest exists: %d - %s\n", pm_exists_dir("ctest"), fails ? "FAILED" : "all OK");
    return fails != 0;
}
