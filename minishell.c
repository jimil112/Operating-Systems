/*********************************************************************
   Program  : miniShell                   Version    : 1.0
 --------------------------------------------------------------------
   POSIX-compliant mini shell with:
     - Background jobs (&)
     - Built-in cd command
     - perror() after system calls
     - Proper termination on exec failure
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

#define MAX_TOKENS 20       /* max number of command tokens */
#define BUF_SIZE 100        /* input buffer size */
#define MAX_BG 128          /* maximum background jobs */

char line[BUF_SIZE];        /* command input buffer */

/* -------- background job tracking -------- */
typedef struct {
    pid_t pid;              /* process id */
    char cmd[BUF_SIZE];     /* command string */
    int active;             /* 1 if job is active, 0 if finished */
    int id;                 /* job number */
} BGTask;

static BGTask tasks[MAX_BG];
static int next_id = 1;

/* Trim trailing whitespace */
void trim(char *s) {
    if (!s) return;
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1]==' ' || s[len-1]=='\t' || s[len-1]=='\n')) {
        s[--len] = '\0';
    }
}

/* shell prompt */
void prompt(void) {
    //fprintf(stdout, "msh> ");
    fflush(stdout);
}

/* add background job to table */
void add_bg(pid_t pid, const char *cmd) {
    for (int i=0;i<MAX_BG;i++) {
        if (!tasks[i].active) {
            tasks[i].active = 1;
            tasks[i].pid = pid;
            strncpy(tasks[i].cmd, cmd, BUF_SIZE-1);
            tasks[i].cmd[BUF_SIZE-1] = '\0';
            tasks[i].id = next_id++;
            printf("[%d] %d\n", tasks[i].id, pid);
            return;
        }
    }
    fprintf(stderr,"Background job table full, pid %d not tracked\n", pid);
}

/* Reap and announce finished jobs */
void check_bg(void) {
    int status;
    pid_t pid;
    for (int i=0;i<MAX_BG;i++) {
        if (tasks[i].active) {
            pid = waitpid(tasks[i].pid,&status,WNOHANG);
            if (pid > 0) {
                printf("[%d]+ Done                 %s\n", tasks[i].id, tasks[i].cmd);
                tasks[i].active = 0;
            } else if (pid == -1 && errno != ECHILD) {
                perror("waitpid");
            }
        }
    }
}

int main(void) {
    int frkRtnVal;          /* value returned by fork sys call */
    char *tokens[MAX_TOKENS]; /* array of pointers to command line tokens */
    char *sep = " \t\n";    /* command line token separators */
    int i;                  /* parse index */

    while (1) {             /* do Forever */
        check_bg();         /* report finished background jobs */
        prompt();           /* show prompt */

        if (!fgets(line, BUF_SIZE, stdin)) {  /* read input */
            if (feof(stdin)) exit(0);        /* non-zero on EOF */
            perror("fgets");
            continue;
        }

        if (line[0]=='\0' || line[0]=='#' || line[0]=='\n')
            continue;       /* to prompt */

        /* save copy of raw command */
        char cmdcopy[BUF_SIZE];
        strncpy(cmdcopy,line,BUF_SIZE-1);
        cmdcopy[BUF_SIZE-1] = '\0';
        trim(cmdcopy);

        /* tokenize input */
        tokens[0] = strtok(line, sep);
        for (i=1;i<MAX_TOKENS-1;i++) {
            tokens[i] = strtok(NULL, sep);
            if (!tokens[i]) break;
        }
        tokens[i] = NULL;
        /* assert i is number of tokens + 1 */

        if (!tokens[0]) continue;

        /* built-in: cd */
        if (strcmp(tokens[0],"cd")==0) {
            const char *dest = (i>1) ? tokens[1] : getenv("HOME");
            if (!dest) {
                fprintf(stderr,"cd: HOME not set\n");
            } else if (chdir(dest)==-1) {
                perror("chdir");
            }
            continue;
        }

        /* check if background job */
        int background = 0;
        int last = 0;
        while (tokens[last]) last++;
        if (last>0 && strcmp(tokens[last-1],"&")==0) {
            background = 1;
            tokens[last-1] = NULL;
            size_t len = strlen(cmdcopy);
            if (len>0 && cmdcopy[len-1]=='&') {
                cmdcopy[len-1] = '\0';
                trim(cmdcopy);
            }
        }

        /* fork a child process to exec the command in tokens[0] */
        frkRtnVal = fork();
        if (frkRtnVal == -1) { /* fork returns error to parent process */
            perror("fork");
            continue;
        }

        if (frkRtnVal == 0) { /* code executed only by child process */
            execvp(tokens[0],tokens);
            perror("execvp");
            _exit(127);
        } else { /* code executed only by parent process */
            if (background) {
                add_bg(frkRtnVal, cmdcopy);
            } else {
                if (waitpid(frkRtnVal,NULL,0)==-1)
                    perror("waitpid");
            }
        } /* switch */
    } /* while */
    return 0;
} /* main */
