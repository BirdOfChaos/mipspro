#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#define MSG_CANNOT_FIND_SERVER \
    "Cannot find SERVER hostname in network database (-14,7:2) No such file or directory"
#define MSG_NO_SUCH_FEATURE "No such feature exists (-5,116)"
#define MSG_GRAPHICS_SUPPORT_CUSTOMER \
    "Graphics support customer then contact your local support provider."

int main(int argc, char *argv[]) {
    char *command;
    char cmd_path[PATH_MAX];
    int pipefd[2];
    pid_t pid;

    /* Retrieve the base name of the executable */
    command = basename(argv[0]);
    if (command == NULL) {
        fprintf(stderr, "Error: basename returned NULL.\n");
        exit(EXIT_FAILURE);
    }

    /* Construct the full path to the command in /usr/bin/ */
    /* Since snprintf is not available in C89, use sprintf with care */
    if (strlen("/usr/bin/") + strlen(command) >= PATH_MAX) {
        fprintf(stderr, "Error: Command path is too long.\n");
        exit(EXIT_FAILURE);
    }
    sprintf(cmd_path, "/usr/bin/%s", command);

    /* Check if the command exists and is executable */
    if (access(cmd_path, X_OK) != 0) {
        fprintf(stderr, "Error: Command '%s' not found or not executable.\n", cmd_path);
        exit(EXIT_FAILURE);
    }

    /* Create a pipe for inter-process communication */
    if (pipe(pipefd) == -1) {
        perror("Error creating pipe");
        exit(EXIT_FAILURE);
    }

    pid = fork();

    if (pid == -1) {
        perror("Error forking process");
        close(pipefd[0]);
        close(pipefd[1]);
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        /* Child process */
        /* Redirect stderr to write end of pipe */
        char **new_argv;
        int i;

        /* Close read end of pipe */
        if (close(pipefd[0]) == -1) {
            perror("Error closing read end of pipe in child");
            exit(EXIT_FAILURE);
        }

        /* Duplicate write end of pipe to stderr */
        if (dup2(pipefd[1], STDERR_FILENO) == -1) {
            perror("Error redirecting stderr in child");
            exit(EXIT_FAILURE);
        }

        /* Close write end of pipe after dup2 */
        if (close(pipefd[1]) == -1) {
            perror("Error closing write end of pipe in child");
            exit(EXIT_FAILURE);
        }

        /* Prepare arguments for exec */
        new_argv = (char **)malloc(sizeof(char *) * (argc + 1));
        if (new_argv == NULL) {
            perror("Error allocating memory for arguments");
            exit(EXIT_FAILURE);
        }

        new_argv[0] = cmd_path;
        for (i = 1; i < argc; i++) {
            new_argv[i] = argv[i];
        }
        new_argv[argc] = NULL;

        /* Execute the command */
        execv(cmd_path, new_argv);

        /* If execv returns, there was an error */
        fprintf(stderr, "Error executing command '%s': %s\n", cmd_path, strerror(errno));
        free(new_argv);
        exit(EXIT_FAILURE);
    } else {
        /* Parent process */
        /* Close write end of pipe */
        FILE *fp;
        int print = 1;
        int first = 0;
        char line[1024];
        int status;
        size_t len;

        if (close(pipefd[1]) == -1) {
            perror("Error closing write end of pipe in parent");
            close(pipefd[0]);
            exit(EXIT_FAILURE);
        }

        /* Read from read end of pipe, process lines, output to stderr */
        fp = fdopen(pipefd[0], "r");
        if (fp == NULL) {
            perror("Error opening pipe for reading");
            close(pipefd[0]);
            exit(EXIT_FAILURE);
        }

        while (fgets(line, sizeof(line), fp) != NULL) {
            /* Remove newline character at end, if any */
            len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
            }

            if (first == 0) {
                if (strcmp(line, MSG_CANNOT_FIND_SERVER) == 0 ||
                    strcmp(line, MSG_NO_SUCH_FEATURE) == 0) {
                    first = 1;
                    print = 0;
                }
            }

            if (print == 1) {
                if (fprintf(stderr, "%s\n", line) < 0) {
                    perror("Error writing to stderr");
                    fclose(fp);
                    exit(EXIT_FAILURE);
                }
            } else {
                if (strcmp(line, MSG_GRAPHICS_SUPPORT_CUSTOMER) == 0) {
                    /* Read one extra (empty) line after the final message, then display everything after that */
                    if (fgets(line, sizeof(line), fp) == NULL) {
                        break;
                    }
                    print = 1;
                }
            }
        }

        if (ferror(fp)) {
            perror("Error reading from pipe");
            fclose(fp);
            exit(EXIT_FAILURE);
        }

        if (fclose(fp) == EOF) {
            perror("Error closing pipe");
            exit(EXIT_FAILURE);
        }

        /* Wait for child process to finish */
        if (waitpid(pid, &status, 0) == -1) {
            perror("Error waiting for child process");
            exit(EXIT_FAILURE);
        }

        /* Return child's exit status */
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "Child process terminated by signal %d.\n", WTERMSIG(status));
            exit(EXIT_FAILURE);
        } else {
            fprintf(stderr, "Child process terminated abnormally.\n");
            exit(EXIT_FAILURE);
        }
    }
}

