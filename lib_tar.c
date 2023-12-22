#include "lib_tar.h"

/**
 * @brief Verifies if we are at the end of the archive
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @return int 1 if we are at the end of the archive, 0 otherwise
 */
int check_end(int tar_fd)
{
    char header_next_header[1024]; // Buffer to read the current header and the next header
    read(tar_fd, header_next_header, 1024);
    int checksum_next_header = 0;
    for (int i = 0; i < 1024; i++)
    {
        checksum_next_header += header_next_header[i];
    }
    lseek(tar_fd, -1024, SEEK_CUR); // Go back to the current header

    // If the next header is empty, we have reached the end of the archive
    if (checksum_next_header == 0)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief Go to the next header
 *
 * @param header The current header
 * @return int The number of bytes to go to the next header
 */
int next_header(tar_header_t *header)
{
    int next = 0;

    next = TAR_INT(header->size) / 512;
    if (TAR_INT(header->size) % 512 != 0)
    {
        next += 1; // If the size is not a multiple of 512, we need to add 1 to the next header position
    }
    next *= 512; // Since next is the number of blocks, we need to multiply it by 512 (the size of a block) to get the next header position

    return next;
}

/**
 * Checks whether the archive is valid.
 *
 * Each non-null header of a valid archive has:
 *  - a magic value of "ustar" and a null,
 *  - a version value of "00" and no null,
 *  - a correct checksum
 *
 * @param tar_fd A file descriptor pointing to the start of a file supposed to contain a tar archive.
 *
 * @return a zero or positive value if the archive is valid, representing the number of non-null headers in the archive,
 *         -1 if the archive contains a header with an invalid magic value,
 *         -2 if the archive contains a header with an invalid version value,
 *         -3 if the archive contains a header with an invalid checksum value
 */
int check_archive(int tar_fd)
{
    char buf[512]; // Buffer to read the header
    int count = 0; // Number of non-null headers
    long next = 0; // Next header position

    int final = 0;

    while (!final)
    {
        // Read the header
        read(tar_fd, buf, 512);
        // Parse the buffer as a tar header
        tar_header_t *header = (tar_header_t *)buf;

        // Check if the magic value is "ustar"
        if (strncmp(header->magic, TMAGIC, TMAGLEN - 1) != 0)
        {
            return -1;
        }
        // Check if the version value is "00"
        if (strncmp(header->version, TVERSION, TVERSLEN) != 0)
        {
            return -2;
        }

        // Check if the checksum is correct
        int checksum = 0;
        for (int i = 0; i < 512; i++)
        {
            if (i < 148 || i > 155) // The checksum field is between 148 and 155 bits
            {
                checksum += buf[i];
            }
            else
            {
                checksum += ' ';
            }
        }
        if (TAR_INT(header->chksum) != checksum)
        {
            return -3;
        }

        next = next_header(header);
        lseek(tar_fd, next, SEEK_CUR);

        final = check_end(tar_fd);

        count++;
    }

    return count;
}

/**
 * Checks whether an entry exists in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive,
 *         any other value otherwise.
 */
int exists(int tar_fd, char *path)
{
    char buf[512]; // Buffer to read the header
    long next = 0; // Next header position

    int final = 0;

    while (!final)
    {
        read(tar_fd, buf, 512);                     // Read the header
        tar_header_t *header = (tar_header_t *)buf; // Parse the buffer as a tar header

        // Check if the path is the same as the one in the header
        if (strncmp(header->name, path, strlen(path)) == 0)
        {
            return 1;
        }

        next = next_header(header);
        lseek(tar_fd, next, SEEK_CUR);

        final = check_end(tar_fd);
    }

    return 0;
}

/**
 * Checks whether an entry exists in the archive and is a directory.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a directory,
 *         any other value otherwise.
 */
int is_dir(int tar_fd, char *path)
{
    char bufer[512];

    long next = 0;
    int final = 0;

    while (!final)
    {
        read(tar_fd, bufer, 512);
        tar_header_t *header = (tar_header_t *)bufer;

        if (strncmp(header->name, path, strlen(path)) == 0)
        {
            if (header->typeflag == DIRTYPE)
            {
                return 1;
            }

            return 0;
        }

        next = next_header(header);
        lseek(tar_fd, next, SEEK_CUR);

        final = check_end(tar_fd);
    }

    return 0;
}

/**
 * Checks whether an entry exists in the archive and is a file.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a file,
 *         any other value otherwise.
 */
int is_file(int tar_fd, char *path)
{
    char bufer[512];

    long next = 0;
    int final = 0;

    while (!final)
    {
        read(tar_fd, bufer, 512);
        tar_header_t *header = (tar_header_t *)bufer;

        if (strncmp(header->name, path, strlen(path)) == 0)
        {
            if (header->typeflag == REGTYPE || header->typeflag == AREGTYPE)
            {
                return 1;
            }

            return 0;
        }

        next = next_header(header);
        lseek(tar_fd, next, SEEK_CUR);

        final = check_end(tar_fd);
    }
    return 0;
}

/**
 * Checks whether an entry exists in the archive and is a symlink.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 * @return zero if no entry at the given path exists in the archive or the entry is not symlink,
 *         any other value otherwise.
 */
int is_symlink(int tar_fd, char *path)
{
    char bufer[512];

    long next = 0;
    int final = 0;

    while (!final)
    {
        read(tar_fd, bufer, 512);
        tar_header_t *header = (tar_header_t *)bufer;

        if (strncmp(header->name, path, strlen(path)) == 0)
        {

            if (header->typeflag == SYMTYPE)
            {
                return 1;
            }

            return 0;
        }

        next = next_header(header);
        lseek(tar_fd, next, SEEK_CUR);

        final = check_end(tar_fd);
    }

    return 0;
}

/**
 * Lists the entries at a given path in the archive.
 * list() does not recurse into the directories listed at the given path.
 *
 * Example:
 *  dir/          list(..., "dir/", ...) lists "dir/a", "dir/b", "dir/c/" and "dir/e/"
 *   ├── a
 *   ├── b
 *   ├── c/
 *   │   └── d
 *   └── e/
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive. If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param entries An array of char arrays, each one is long enough to contain a tar entry path.
 * @param no_entries An in-out argument.
 *                   The caller set it to the number of entries in `entries`.
 *                   The callee set it to the number of entries listed.
 *
 * @return zero if no directory at the given path exists in the archive,
 *         any other value otherwise.
 */
int list(int tar_fd, char *path, char **entries, size_t *no_entries) // -> doesn't work
{
    lseek(tar_fd, 0, SEEK_SET); // Go back to the beginning of the archive if we called list() before

    char buf[512];  // Buffer to read the header
    long next = 0;  // Next header position
    int count = -1; // Number of entries
    int home = 0;   // The position of the deepest directory we went in

    int final = 0;

    while (!final)
    {
        read(tar_fd, buf, 512);                     // Read the header
        tar_header_t *header = (tar_header_t *)buf; // Parse the buffer as a tar header

        char *name = header->name;
        if (header->typeflag == SYMTYPE) // If the entry is a symlink, we need to add a '/' at the end of the name
        {
            strcat(name, "/");
        }

        // Check if the path is the same as the one in the header
        if (strncmp(name, path, strlen(path)) == 0)
        {
            if (header->typeflag == SYMTYPE) // If the entry is a symlink, we need to resolve it
            {
                if (count == -1) // In case we didn't go in a directory before
                {
                    return list(tar_fd, header->linkname, entries, no_entries);
                }
            }

            // We need to remember the deepest directory we went in
            int depth = 0;
            for (int i = 0; i < strlen(header->name); ++i)
            {
                if (header->name[i] == '/')
                {
                    depth = i;
                }
            }
            if (count == -1) // In case we didn't go in a directory before
            {
                home = depth;
                count++;
            }
            else
            {
                if (depth > home) // If we went deeper in the directory, we need to add the entry
                {
                    if (header->name[depth + 1] == '\0') // If the name ends with a '/', we need to add the entry
                    {
                        if (count < *no_entries) // If we still have space in the array, we add the entry
                        {
                            memcpy(entries[count], name, strlen(name));
                            count++;
                        }
                    }
                }
                else
                {
                    if (count < *no_entries) // If we still have space in the array, we add the entry
                    {
                        memcpy(entries[count], name, strlen(name));
                        count++;
                    }
                }
            }
        }

        next = next_header(header);
        lseek(tar_fd, next, SEEK_CUR);

        final = check_end(tar_fd);
    }

    if (count == -1)
    {
        count++;
    }

    *no_entries = count;
    return *no_entries;
}

/**
 * @brief Reads a symlink at a given path in the archive. Same as read_file() but for symlinks.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive to read from. In this function, the path is always a symlink.
 * @param offset An offset in the file from which to start reading from, zero indicates the start of the file.
 * @param dest A destination buffer to read the given file into.
 * @param len An in-out argument.
 *            The caller set it to the size of dest.
 *            The callee set it to the number of bytes written to dest.
 *
 * @return -1 if no entry at the given path exists in the archive or the entry is not a file,
 *         -2 if the offset is outside the file total length,
 *         zero if the file was read in its entirety into the destination buffer,
 *         a positive value if the file was partially read, representing the remaining bytes left to be read to reach
 *         the end of the file.
 *
 */
ssize_t read_symlink(int tar_fd, char *path, size_t offset, uint8_t *dest, size_t *len)
{
    char buf[512];
    long next = 0;

    int final = 0;

    while (!final)
    {
        read(tar_fd, buf, 512);
        tar_header_t *header = (tar_header_t *)buf;

        if (strstr(header->name, path) != NULL) // If the path is in the name, we need to read the file
        {
            // Check the flags
            if (header->typeflag == DIRTYPE || !(header->typeflag == REGTYPE || header->typeflag == AREGTYPE)) // If the entry is a directory or not a file, we need to return -1
            {
                return -1;
            }
            if (header->typeflag == SYMTYPE) // If the entry is a symlink, we need to resolve it
            {
                return read_symlink(tar_fd, header->linkname, offset, dest, len);
            }

            // Check the offset
            ssize_t size = TAR_INT(header->size);
            if (offset > size) // If the offset is outside the file total length, we need to return -2
            {
                return -2;
            }
            lseek(tar_fd, offset, SEEK_CUR); // Go to the offset position
            if ((size - offset) < *len)      // If the remaining bytes to read are less than the size of the buffer, we need to return the remaining bytes
            {
                *len = size - offset;
            }
            read(tar_fd, dest, *len); // Read the file
            return (size - offset) - *len;
        }

        // Check the number of bytes to read
        next = next_header(header);
        lseek(tar_fd, next, SEEK_CUR);

        final = check_end(tar_fd);
    }

    return -1;
}

/**
 * Reads a file at a given path in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive to read from.  If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param offset An offset in the file from which to start reading from, zero indicates the start of the file.
 * @param dest A destination buffer to read the given file into.
 * @param len An in-out argument.
 *            The caller set it to the size of dest.
 *            The callee set it to the number of bytes written to dest.
 *
 * @return -1 if no entry at the given path exists in the archive or the entry is not a file,
 *         -2 if the offset is outside the file total length,
 *         zero if the file was read in its entirety into the destination buffer,
 *         a positive value if the file was partially read, representing the remaining bytes left to be read to reach
 *         the end of the file.
 *
 */
ssize_t read_file(int tar_fd, char *path, size_t offset, uint8_t *dest, size_t *len)
{
    char buf[512];
    long next = 0;

    int final = 0;

    while (!final)
    {
        read(tar_fd, buf, 512);
        tar_header_t *header = (tar_header_t *)buf;

        if (strncmp(header->name, path, strlen(path)) == 0)
        {
            // Check the flags
            if (header->typeflag == DIRTYPE) // If the entry is a directory, we need to return -1
            {
                return -1;
            }
            if (header->typeflag == SYMTYPE) // If the entry is a symlink, we need to resolve it
            {
                return read_symlink(tar_fd, header->linkname, offset, dest, len);
            }
            if (!(header->typeflag == REGTYPE || header->typeflag == AREGTYPE)) // If the entry is not a file, we need to return -1
            {
                return -1;
            }

            // Check the offset
            ssize_t size = TAR_INT(header->size);
            if (offset > size) // If the offset is outside the file total length, we need to return -2
            {
                return -2;
            }
            lseek(tar_fd, offset, SEEK_CUR); // Go to the offset position
            if ((size - offset) < *len)      // If the remaining bytes to read are less than the size of the buffer, we need to return the remaining bytes
            {
                *len = size - offset;
            }
            read(tar_fd, dest, *len); // Read the file
            return (size - offset) - *len;
        }

        // Check the number of bytes to read
        next = next_header(header);
        lseek(tar_fd, next, SEEK_CUR);

        final = check_end(tar_fd);
    }

    return -1;
}
