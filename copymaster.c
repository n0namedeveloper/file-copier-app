#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "options.h"
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <stdbool.h>

// Function to display parsed options from the command line
void PrintCopymasterOptions(struct CopymasterOptions* cpm_options) {
    printf("Parsed options:\n");
    printf("Infile: %s\n", cpm_options->infile);
    printf("Outfile: %s\n", cpm_options->outfile);
    printf("Overwrite: %d\n", cpm_options->overwrite);
    printf("Append: %d\n", cpm_options->append);
    printf("Create: %d\n", cpm_options->create);
    printf("Delete option: %d\n", cpm_options->delete_opt);
    printf("Link: %d\n", cpm_options->link);
    printf("Chmod: %d\n", cpm_options->chmod);
    printf("Fast mode: %d\n", cpm_options->fast);
    printf("Slow mode: %d\n", cpm_options->slow);
    printf("Lseek: %d\n", cpm_options->lseek);
    printf("Inode: %d\n", cpm_options->inode);
    printf("Create mode: %o\n", cpm_options->create_mode);
    printf("Chmod mode: %o\n", cpm_options->chmod_mode);
    printf("Inode number: %ld\n", cpm_options->inode_number);
}

// Error handling function
void FatalError(char c, const char* msg, int exit_status) {
    fprintf(stderr, "Error %c: %s\n", c, msg);
    exit(exit_status);
}

// Main function
int main(int argc, char* argv[]) {
    // Parse command-line options into the struct
    struct CopymasterOptions cpm_options = ParseCopymasterOptions(argc, argv);

    // Display parsed options for verification
    PrintCopymasterOptions(&cpm_options);

    // Check for conflicting options
    if (cpm_options.fast && cpm_options.slow && cpm_options.create && cpm_options.delete_opt) {
        fprintf(stderr, "KONFLIKT PREPINACOV.\n");
        exit(EXIT_FAILURE);
    }

    // Handle hard link creation
    if (cpm_options.link) {
        struct stat infile_stat;

        if (stat(cpm_options.infile, &infile_stat) < 0) {
            if (errno == ENOENT) {
                fprintf(stderr, "VSTUPNY SUBOR NEEXISTUJE\n");
                exit(30);
            } else {
                perror("INA CHYBA");
                exit(30);
            }
        }

        if (access(cpm_options.outfile, F_OK) == 0) {
            fprintf(stderr, "VYSTUPNY SUBOR UZ EXISTUJE\n");
            exit(30);
        }

        if (link(cpm_options.infile, cpm_options.outfile) < 0) {
            perror("INA CHYBA");
            exit(30);
        }

        printf("Hard link successfully created.\n");
        return 0;
    }

    // Validate input file existence
    struct stat infile_stat;
    if (stat(cpm_options.infile, &infile_stat) < 0) {
        if (errno == ENOENT) {
            fprintf(stderr, "SUBOR NEEXISTUJE\n");
            exit(21);
        } else {
            perror("INA CHYBA");
            exit(21);
        }
    }

    // Open input file in read-only mode
    int infile_fd = open(cpm_options.infile, O_RDONLY);
    if (infile_fd < 0) {
        perror("INA CHYBA");
        exit(21);
    }

    // Determine flags and mode for opening the output file
    int open_flags = O_WRONLY | O_CREAT;
    mode_t mode = infile_stat.st_mode;

    if (cpm_options.overwrite) open_flags |= O_TRUNC;
    if (cpm_options.append) open_flags |= O_APPEND;

    if (cpm_options.create) {
        if (stat(cpm_options.outfile, &infile_stat) == 0) {
            fprintf(stderr, "SUBOR EXISTUJE\n");
            close(infile_fd);
            exit(23);
        }

        if (cpm_options.create_mode > 0777) {
            fprintf(stderr, "ZLE PRAVA\n");
            close(infile_fd);
            exit(23);
        }
        mode = cpm_options.create_mode;
    }

    // Additional checks on output file for append/overwrite modes
    struct stat outfile_stat;
    if (cpm_options.append) {
        if (stat(cpm_options.outfile, &outfile_stat) != 0 || access(cpm_options.outfile, W_OK) != 0) {
            fprintf(stderr, "INA CHYBA\n");
            close(infile_fd);
            exit(22);
        }
    } else if (cpm_options.overwrite) {
        if (stat(cpm_options.outfile, &outfile_stat) != 0 && errno == ENOENT) {
            fprintf(stderr, "OUTFILE НЕ ІСНУЄ\n");
            close(infile_fd);
            exit(24);
        }
    }

    // Open the output file
    int outfile_fd = open(cpm_options.outfile, open_flags, mode);
    if (outfile_fd < 0) {
        perror("INA CHYBA");
        close(infile_fd);
        exit(21);
    }

    // Buffer setup for file copy
    char buffer[4096];
    ssize_t bytes_read, bytes_written;

    // File transfer based on speed options
    if (cpm_options.fast) {
        while ((bytes_read = read(infile_fd, buffer, sizeof(buffer))) > 0) {
            bytes_written = write(outfile_fd, buffer, bytes_read);
            if (bytes_written != bytes_read) {
                perror("INA CHYBA");
                close(infile_fd);
                close(outfile_fd);
                exit(21);
            }
        }
        if (bytes_read < 0) {
            perror("INA CHYBA");
        }
    } else if (cpm_options.slow) {
        char byte;
        while ((bytes_read = read(infile_fd, &byte, 1)) > 0) {
            bytes_written = write(outfile_fd, &byte, 1);
            if (bytes_written != bytes_read) {
                perror("INA CHYBA");
                close(infile_fd);
                close(outfile_fd);
                exit(21);
            }
        }
        if (bytes_read < 0) {
            perror("INA CHYBA");
        }
    } else {
        while ((bytes_read = read(infile_fd, buffer, sizeof(buffer))) > 0) {
            bytes_written = write(outfile_fd, buffer, bytes_read);
            if (bytes_written != bytes_read) {
                perror("INA CHYBA");
                close(infile_fd);
                close(outfile_fd);
                exit(21);
            }
        }
        if (bytes_read < 0) {
            perror("INA CHYBA");
        }
    }

    // Handle lseek option
    if (cpm_options.lseek) {
        if (lseek(infile_fd, cpm_options.lseek_options.pos1, cpm_options.lseek_options.x) == -1) {
            perror("Chyba pri prvom presúvaní súboru (pos1)");
            close(infile_fd);
            close(outfile_fd);
            exit(EXIT_FAILURE);
        }

        if (cpm_options.lseek_options.pos2 != -1) {
            if (lseek(infile_fd, cpm_options.lseek_options.pos2, cpm_options.lseek_options.x) == -1) {
                perror("Chyba pri druhom presune súboru (pos2)");
                close(infile_fd);
                close(outfile_fd);
                exit(EXIT_FAILURE);
            }
        }
    }

    // Validate inode number if required
    if (cpm_options.inode) {
        struct stat file_stat;
        if (fstat(infile_fd, &file_stat) == -1) {
            perror("ZLY TYP VSTUPNEHO SUBORU ");
            close(infile_fd);
            exit(EXIT_FAILURE);
        }
        if (file_stat.st_ino != cpm_options.inode_number) {
            fprintf(stderr, "ZLY INODE\n");
            close(infile_fd);
            exit(27);
        }
    }

    // Handle delete option, removing input file after copying
    if (cpm_options.delete_opt) {
        if (unlink(cpm_options.infile) < 0) {
            perror("Chyba pri odstraňovaní súboru");
            close(infile_fd);
            close(outfile_fd);
            exit(EXIT_FAILURE);
        }
        printf("Súbor %s bol úspešne odstránený.\n", cpm_options.infile);
    }

    // Modify output file permissions if chmod option is set
    if (cpm_options.chmod) {
        if (chmod(cpm_options.outfile, cpm_options.chmod_mode) == -1) {
            perror("Chyba pri zmene prístupových práv k súboru");
            close(infile_fd);
            close(outfile_fd);
            exit(EXIT_FAILURE);
        }
        printf("Prístupové práva k %s boli úspešne zmenené na %o.\n", cpm_options.outfile, cpm_options.chmod_mode);
    }

    // Close file descriptors before exiting
    close(infile_fd);
    close(outfile_fd);

    printf("Kopírovanie súborov bolo úspešné.\n");
    return 0;
}
