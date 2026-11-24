/*
 * countlines.c - run a command and count the lines it prints
 *
 * Usage: countlines COMMAND [ARG]...
 *
 * Runs COMMAND with its standard output connected to a pipe, counts the
 * lines that come through the pipe, waits for COMMAND to finish, and prints
 *
 *     lines: N
 *     exit status: S
 *
 * If COMMAND can't be run, the child prints an error with perror and exits
 * with status 127 (what a shell uses for "command not found").
 *
 * You may use execvp in this lab, so COMMAND can be a name like ls instead
 * of a full path like /bin/ls.
 *
 * STEP 4 of the writeup walks through how pipe, fork, dup2, and execvp fit
 * together, with a figure of both processes' fd tables.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/** How many bytes to read at a time */
#define BUFFER_SIZE 4096

/**
 * Creates a pipe and starts the command argv[0] (with arguments argv) in a
 * child process whose standard output is the pipe's write end. Stores the
 * pipe's read end in *read_fd.
 *
 * Returns the child's process ID, or -1 on error.
 *
 * Hints:
 * - If pipe or fork fails, there is no child yet. Print an error with perror
 *   and return -1.
 * - The child should never return from this function. If it did, it would
 *   go back to main and keep running as a second copy of countlines. So if
 *   dup2 or execvp fails, print an error with perror and exit. (execvp only
 *   returns when it fails.)
 * - After dup2, the child's standard output is the pipe, so anything the
 *   child prints with printf goes into the pipe and gets counted. Print
 *   debugging messages to stderr instead: fprintf(stderr, ...).
 *
 */
static pid_t start_command(char *argv[], int *read_fd) {
    int fds[2];
    if (pipe(fds) < 0) {
        perror("pipe");
        return -1;
    }

    pid_t child_pid = fork();
    if (child_pid < 0) {
        perror("fork");
        close(fds[0]);
        close(fds[1]);
        return -1;
    }

    if (child_pid == 0) {
        if (dup2(fds[1], STDOUT_FILENO) < 0) {
            perror("dup2");
            _exit(127);
        }
        close(fds[0]);
        close(fds[1]);
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }

    close(fds[1]);
    *read_fd = fds[0];
    return child_pid;
}

/**
 * Reads from fd until end of file.
 *
 * Returns the number of newline characters read.
 *
 * Hint: this is the same read loop as copy_fd in mycat. read can return fewer
 * bytes than the buffer holds, so only look at the bytes it returned.
 */
static int count_lines(int fd) {
    char buffer[BUFFER_SIZE];
    int lines = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (buffer[i] == '\n') {
                lines++;
            }
        }
    }
    return bytes_read < 0 ? -1 : lines;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: countlines COMMAND [ARG]...\n");
        return 1;
    }

    int read_fd;
    pid_t child_pid = start_command(argv + 1, &read_fd);
    if (child_pid < 0) {
        return 1;
    }

    int lines = count_lines(read_fd);
    if (lines < 0) {
        perror("read");
    }
    close(read_fd);

    int wait_status;
    if (waitpid(child_pid, &wait_status, 0) < 0) {
        perror("waitpid");
        return 1;
    }
    if (lines < 0) {
        return 1;
    }

    int exit_status = WIFEXITED(wait_status)
        ? WEXITSTATUS(wait_status)
        : 128 + WTERMSIG(wait_status);
    printf("lines: %d\nexit status: %d\n", lines, exit_status);
    return 0;
}
