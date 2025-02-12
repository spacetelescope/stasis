#include "copy.h"

int copy2(const char *src, const char *dest, unsigned int op) {
    struct stat src_stat, dnamest;

    SYSDEBUG("Stat source file: %s", src);
    if (lstat(src, &src_stat) < 0) {
        perror(src);
        return -1;
    }

    if (access(dest, F_OK) == 0) {
        unlink(dest);
    }

    char dname[1024] = {0};
    strcpy(dname, dest);

    char *dname_endptr = strrchr(dname, '/');
    if (dname_endptr != NULL) {
        *dname_endptr = '\0';
    }

    SYSDEBUG("Stat destination file: %s", dname);
    stat(dname, &dnamest);

    if (S_ISLNK(src_stat.st_mode)) {
        char lpath[1024] = {0};
        if (readlink(src, lpath, sizeof(lpath)) < 0) {
            perror(src);
            return -1;
        }
        if (symlink(lpath, dest) < 0) {
            // silent
            return -1;
        }
    } else if (S_ISREG(src_stat.st_mode) && src_stat.st_nlink > 2 && src_stat.st_dev == dnamest.st_dev) {
        if (link(src, dest) < 0) {
            perror(src);
            return -1;
        }
    } else if (S_ISFIFO(src_stat.st_mode) || S_ISBLK(src_stat.st_mode) || S_ISCHR(src_stat.st_mode) || S_ISSOCK(src_stat.st_mode)) {
        if (mknod(dest, src_stat.st_mode, src_stat.st_rdev) < 0) {
            perror(src);
            return -1;
        }
    } else if (S_ISREG(src_stat.st_mode)) {
        char buf[STASIS_BUFSIZ] = {0};
        size_t bytes_read;
        SYSDEBUG("%s", "Opening source file for reading");
        FILE *fp1 = fopen(src, "rb");
        if (!fp1) {
            perror(src);
            return -1;
        }

        SYSDEBUG("%s", "Opening destination file for writing");
        FILE *fp2 = fopen(dest, "w+b");
        if (!fp2) {
            perror(dest);
            return -1;
        }

        size_t bytes_written = 0;
        while ((bytes_read = fread(buf, sizeof(char), sizeof(buf), fp1)) != 0) {
            bytes_written += fwrite(buf, sizeof(char), bytes_read, fp2);
        }
        fclose(fp1);
        fclose(fp2);

        if (bytes_written != (size_t) src_stat.st_size) {
            fprintf(stderr, "%s: SHORT WRITE (expected %zu bytes, but wrote %zu bytes)\n", dest, src_stat.st_size, bytes_written);
            return -1;
        }

        if (op & CT_OWNER && chown(dest, src_stat.st_uid, src_stat.st_gid) < 0) {
            perror(dest);
        }

        if (op & CT_PERM && chmod(dest, src_stat.st_mode) < 0) {
            perror(dest);
        }
    } else {
        errno = EOPNOTSUPP;
        return -1;
    }
    SYSDEBUG("%s", "Data copied");
    return 0;
}
