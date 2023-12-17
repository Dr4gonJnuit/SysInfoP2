#include "lib_tar.h"

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

    while (1)
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

        next = TAR_INT(header->size) / 512;
        if (TAR_INT(header->size) % 512 != 0)
        {
            next += 1; // If the size is not a multiple of 512, we need to add 1 to the next header position
        }
        next *= 512; // Since next is the number of blocks, we need to multiply it by 512 (the size of a block) to get the next header position
        lseek(tar_fd, next, SEEK_CUR);

        // Check if the next header is empty
        char header_next_header[1024]; // Buffer to read the current header and the next header
        read(tar_fd, header_next_header, 1024);
        int checksum_next_header = 0;
        for (int i = 0; i < 1024; i++)
        {
            checksum_next_header += header_next_header[i];
        }
        lseek(tar_fd, -1024, SEEK_CUR); // Go back to the current header

        count++;

        // If the next header is empty, we have reached the end of the archive
        if (checksum_next_header == 0)
        {
            break;
        }
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

    while (1)
    {
        read(tar_fd, buf, 512);                     // Read the header
        tar_header_t *header = (tar_header_t *)buf; // Parse the buffer as a tar header

        // Check if the path is the same as the one in the header
        if (strncmp(header->name, path, strlen(path)) == 0)
        {
            return 1;
        }

        next = TAR_INT(header->size) / 512;
        if (TAR_INT(header->size) % 512 != 0)
        {
            next += 1; // If the size is not a multiple of 512, we need to add 1 to the next header position
        }
        next *= 512; // Since next is the number of blocks, we need to multiply it by 512 (the size of a block) to get the next header position
        lseek(tar_fd, next, SEEK_CUR);

        // Check if the next header is empty
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
            break;
        }
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
int list(int tar_fd, char *path, char **entries, size_t *no_entries)
{
    return 0;
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
    return 0;
}