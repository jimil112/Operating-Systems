/*********************************************************************
   Program  : miniShell                   Version    : 1.0
 --------------------------------------------------------------------
   POSIX-compliant mini shell with:
     - Background job support (&)
     - Built-in cd command
     - Proper perror() for system calls
     - Child termination on exec failure
 --------------------------------------------------------------------
   File            : minishell.c
   Compiler/System : gcc/linux
********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_ARGS 20
#define BUF_SIZE 100
#define MAX_BG_JOBS 128

typedef struct {
    pid_t pid;
    char cmd[BUF_SIZE];
    int active;
    int job_id;
} BackgroundJob;

static BackgroundJob bg_jobs[MAX_BG_JOBS];
static int next_job_id = 1;

void trim_newline(char *s) {
    if (!s) return;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == ' ' || s[len-1] == '\t')) {
        s[--len] = '\0';
    }
}

/*
    shell prompt
*/
void prompt(void) {
    //fprintf(stdout, "msh> ");
    fflush(stdout);
}

/* add a background job to table */
void add_bg_job(pid_t pid, const char *cmd) {
    for (int i = 0; i < MAX_BG_JOBS; i++) {
        if (!bg_jobs[i].active) {
            bg_jobs[i].pid = pid;
            strncpy(bg_jobs[i].cmd, cmd, BUF_SIZE-1);
            bg_jobs[i].cmd[BUF_SIZE-1] = '\0';
            bg_jobs[i].active = 1;
            bg_jobs[i].job_id = next_job_id++;
            printf("[%d] %d\n", bg_jobs[i].job_id, pid);
            return;
        }
    }
    fprintf(stderr, "Warning: background job table full, pid %d not tracked\n", pid);
}

/* reap finished background jobs */
void check_bg_jobs() {
    int status;
    pid_t pid;
    for (int i = 0; i < MAX_BG_JOBS; i++) {
        if (bg_jobs[i].active) {
            pid = waitpid(bg_jobs[i].pid, &status, WNOHANG);
            if (pid == -1 && errno != ECHILD) {
                perror("waitpid");
            } else if (pid > 0) {
                printf("[%d]+ Done                 %s\n", bg_jobs[i].job_id, bg_jobs[i].cmd);
                bg_jobs[i].active = 0;
            }
        }
    }
}

int main(void) {
    int frkRtnVal;          /* value returned by fork sys call */
    char line[BUF_SIZE];    /* command input buffer */
    char *args[MAX_ARGS];   /* array of pointers to command line tokens */
    char *sep = " \t\n";    /* command line token separators */
    int i;                  /* parse index */

    while (1) {             /* do Forever */
        check_bg_jobs();    /* report finished background jobs */
        prompt();           /* show prompt */

        if (fgets(line, BUF_SIZE, stdin) == NULL) {  /* read input */
            if (feof(stdin)) exit(0);               /* non-zero on EOF */
            perror("fgets");
            continue;
        }

        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') {
            continue; /* to prompt */
        }

        /* save copy of raw command */
        char cmd_copy[BUF_SIZE];
        strncpy(cmd_copy, line, BUF_SIZE-1);
        cmd_copy[BUF_SIZE-1] = '\0';
        trim_newline(cmd_copy);

        /* tokenize input */
        args[0] = strtok(line, sep);
        for (i = 1; i < MAX_ARGS-1; i++) {
            args[i] = strtok(NULL, sep);
            if (args[i] == NULL) break;
        }
        args[i] = NULL;
        /* assert i is number of tokens + 1 */

        if (!args[0]) continue;

        /* built-in: cd */
        if (strcmp(args[0], "cd") == 0) {
            const char *dir = (i > 1) ? args[1] : getenv("HOME");
            if (!dir) {
                fprintf(stderr, "cd: HOME not set\n");
            } else if (chdir(dir) == -1) {
                perror("chdir");
            }
            continue;
        }

        /* check if background job */
        int background = 0;
        int last = 0;
        while (args[last] != NULL) last++;
        if (last > 0 && strcmp(args[last-1], "&") == 0) {
            background = 1;
            args[last-1] = NULL;
            size_t L = strlen(cmd_copy);
            if (L > 0 && cmd_copy[L-1] == '&') {
                cmd_copy[L-1] = '\0';
                trim_newline(cmd_copy);
            }
        }

        /* fork a child process to exec the command in args[0] */
        frkRtnVal = fork();
        switch (frkRtnVal) {
            case -1: /* fork returns error to parent process */
                perror("fork");
                continue;

            case 0:  /* code executed only by child process */
                execvp(args[0], args);
                perror("execvp");
                _exit(127);

            default: /* code executed only by parent process */
                if (background) {
                    add_bg_job(frkRtnVal, cmd_copy);
                } else {
                    if (waitpid(frkRtnVal, NULL, 0) == -1)
                        perror("waitpid");
                }
                break;
        } /* switch */
    } /* while */
    return 0;
} /* main */
