#include "copy.h"

int copy2(const char *src, const char *dest, unsigned int op) {
    size_t bytes_read;
    size_t bytes_written;
    char buf[OMC_BUFSIZ];
    struct stat src_stat, dnamest;
    FILE *fp1, *fp2;

    if (lstat(src, &src_stat) < 0) {
        perror(src);
        return -1;
    }

    if (access(dest, F_OK) == 0) {
        unlink(dest);
    }

    char dname[1024] = {0};
    strcpy(dname, dest);
    char *dname_endptr;

    dname_endptr = strrchr(dname, '/');
    if (dname_endptr != NULL) {
        *dname_endptr = '\0';
    }

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
        fp1 = fopen(src, "rb");
        if (!fp1) {
            perror(src);
            return -1;
        }

        fp2 = fopen(dest, "w+b");
        if (!fp2) {
            perror(dest);
            return -1;
        }

        bytes_written = 0;
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
    return 0;
}

int mkdirs(const char *_path, mode_t mode) {
    int status;
    char *token;
    char pathbuf[1024] = {0};
    char *path;
    path = pathbuf;
    strcpy(path, _path);
    errno = 0;

    char result[1024] = {0};
    status = 0;
    while ((token = strsep(&path, "/")) != NULL && !status) {
        if (token[0] == '.')
            continue;
        strcat(result, token);
        strcat(result, "/");
        status = mkdir(result, mode);
        if (status && errno == EEXIST) {
            status = 0;
            errno = 0;
            continue;
        }
    }
    return status;
}

int copytree(const char *srcdir, const char *destdir, unsigned int op) {
    DIR *dir;
    struct dirent *d;
    struct stat st;
    mode_t mode;
    mode_t umask_value;
    errno = 0;

    dir = opendir(srcdir);
    if (!dir) {
        return -1;
    }

    umask_value = umask(0);
    umask(umask_value);

    if (lstat(srcdir, &st) < 0) {
        perror(srcdir);
        closedir(dir);
        return -1;
    }

    if (op & CT_PERM) {
        mode = st.st_mode;
    } else {
        mode = 0777 & ~umask_value;
    }

    // Source directory is not writable so modify the mode for the destination
    if (mode & ~S_IWUSR) {
        mode |= S_IWUSR;
    }

    if (access(destdir, F_OK) < 0) {
        if (mkdirs(destdir, mode) < 0) {
            perror(destdir);
            closedir(dir);
            return -1;
        }
    }

    memset(&st, 0, sizeof(st));
    while ((d = readdir(dir)) != NULL) {
        char src[1024] = {0};
        char dest[1024] = {0};
        if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0) {
            continue;
        }
        strcat(src, srcdir);
        strcat(src, "/");
        strcat(src, d->d_name);

        strcat(dest, destdir);
        strcat(dest, "/");
        strcat(dest, d->d_name);

        if (lstat(src, &st) < 0) {
            perror(srcdir);
            closedir(dir);
            return -1;
        }

        if (d->d_type == DT_DIR) {
            if (strncmp(src, dest, strlen(src)) == 0) {
                closedir(dir);
                return -1;
            }

            if (access(dest, F_OK) < 0) {
                if (mkdir(dest, mode) < 0) {
                    perror(dest);
                    closedir(dir);
                    return -1;
                }
            }

            copytree(src, dest, op);
        }
        copy2(src, dest, op);
    }
    closedir(dir);
    return 0;
}
