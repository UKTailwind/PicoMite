#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <picomite/basic.h>
#include <picomite/files.h>

int main(void)
{
    char cwd[128], name[64];
    struct stat st;
    DIR *d;
    struct dirent *e;
    FILE *f;

    printf("cwd %s, drive %s, free %lld\n", getcwd(cwd, sizeof cwd), pm_drive_name(name, sizeof name), pm_free_space());
    mkdir("ctest", 0);
    f = fopen("ctest/log.txt", "w"); fputs("one\n", f); fclose(f);
    f = fopen("ctest/log.txt", "a"); fputs("two\n", f); fclose(f);
    f = fopen("ctest/log.txt", "r+"); fseek(f, 0, SEEK_SET); fputs("ONE", f);
    fseek(f, 0, SEEK_SET); fgets(name, sizeof name, f); printf("r+: %s", name); fclose(f);
    f = fopen("ctest/log.txt", "a+"); fputs("three\n", f); rewind(f);
    while (fgets(name, sizeof name, f)) printf("a+: %s", name);
    fclose(f);
    if (stat("ctest/log.txt", &st) == 0)
        printf("size %ld %s mtime %lld\n", st.st_size, S_ISREG(st.st_mode) ? "file" : "dir", (long long)st.st_mtime);
    if (rename("ctest/log.txt", "ctest/log2.txt")) printf("rename: %s\n", pm_errmsg());
    if ((d = opendir("ctest")) != NULL)
    {
        while ((e = readdir(d)) != NULL)
            printf("  %s%s\n", e->d_name, e->d_type == DT_DIR ? "/" : "");
        closedir(d);
    }
    if (remove("nonexistent.txt")) printf("remove: %s\n", pm_errmsg());
    remove("ctest/log2.txt");
    if (rmdir("ctest")) printf("rmdir: %s\n", pm_errmsg());
    printf("exists %d\n", pm_exists_dir("ctest"));
    return 0;
}
