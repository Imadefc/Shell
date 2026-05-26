//------------------------------------------------------------------------------
// UNIX Shell Project
// 
// Sistemas Operativos
// Dept. Arquitectura de Computadores - UMA
// 
// To compile and run the program:
//    $ gcc Shell_project.c parse_line.c list.c job_control.c -o Shell
//    $ ./Shell          
//    ShellSO > 
//     (then type ^D to exit program)
//------------------------------------------------------------------------------

// standard headers
#include <stdio.h>          // printf, stderr, perror, fprintf
#include <stdlib.h>         // malloc, free
//#include <malloc.h>
#include <string.h>         // strcmp
#include <fcntl.h>          // open
#include <unistd.h>         // fork, execvp, tcgetpgrp, dup2, close
//#include <termios.h>
#include <signal.h>         // signal
#include <sys/wait.h>       // waitpid
//#include <sys/types.h>
#include <errno.h>          // errno

// local project headers
#include "parse_line.h"     // link with parse_line.o
#include "job_control.h"    // link with job_control.o and list.o


// -----------------------------------------------------------------------------
//                            Global data structures
// -----------------------------------------------------------------------------
// Declara aqui las variables globales que tengan que ser accedidas desde los
//  manejadores establecidos con signal() o sigaction()i
list_head_t * listaProcesos; 

// -----------------------------------------------------------------------------
// Useful functions to deal with signal handlers and signal masks
// -----------------------------------------------------------------------------
// set a handler (SIG_IGN or SIG_DFL) for signal sent by terminal
void terminal_signals(void (*func)(int))
{
    signal(SIGINT,  func); // crtl+c interrupt tecleado en el terminal
    signal(SIGQUIT, func); // ctrl+\ quit tecleado en el terminal
    signal(SIGTSTP, func); // crtl+z Stop tecleado en el terminal
    signal(SIGTTIN, func); // proceso en segundo plano quiere leer del terminal
    signal(SIGTTOU, func); // proceso en segundo plano quiere escribir en el terminal
}
// -----------------------------------------------------------------------------
// mask or unmask a given signal depending on the block argument
void mask_signal(int signal, int block)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, signal);
    sigprocmask(block, &mask, NULL); // block: SIG_BLOCK/SIG_UNBLOCK
}
//------------------------------------------------------------------------------

void myHandler(int signal){
  int wstatus;
  int pid_wait;
  while (pid_wait = waitpid(-1,&wstatus,WNOHANG | WUNTRACED | WCONTINUED)){
    if(pid_wait== -1){
      if(errno == ECHILD)break;
      perror("wait");
    }
    else{
              job * aux = get_job_bypid(listaProcesos, pid_wait);

              if(WIFSIGNALED(wstatus)){
                printf("[%d] (%s) Signaled by signal %d\n",pid_wait,aux->command, WTERMSIG(wstatus));
                remove_item(listaProcesos,get_job_bypid(listaProcesos,pid_wait));
              }
              if(WIFEXITED(wstatus)){
                printf("[%d] (%s) Terminated by signal %d\n", pid_wait,aux->command,WEXITSTATUS(wstatus));
                remove_item(listaProcesos,get_job_bypid(listaProcesos,pid_wait));
              }
              if(WIFSTOPPED(wstatus)) {
                printf("[%d] Stopped by Signal:  %d\n", pid_wait,WSTOPSIG(wstatus));
                aux->state = STOPPED;
              }
              if(WIFCONTINUED(wstatus)){
                   printf("[%d] Continued .", pid_wait);
                   aux->state = BACKGROUND;
              }
    }
  }
}
   // -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
//                            MAIN          
// -----------------------------------------------------------------------------
int main(void)
{
    char **argv = NULL;
    int argc;
    // probably useful variables:
    int background;             // equals 1 if a command is followed by '&'
    int pid_fork, pid_wait;     // pid for created and waited process
    int wstatus;           // status returned by waitpid
    listaProcesos = new_list("Jobs");
    char *file_in, *file_out;   // for redirections
    terminal_signals(SIG_IGN);
    int pid_terminal = getpid();
    signal(SIGCHLD, myHandler); //manejador de la señal sigchild
    while (1) {
        free_argv(argv);
        if(file_in!=NULL) free(file_in);
        if (file_out!=NULL) { free(file_out);
        }
        int ret = get_command("ShellSO > ", &argc, &argv);
        if (ret == -1) exit(EXIT_FAILURE);      // error in read(2)
        if (ret == 0) break;                    // finish loop if ^D (eof)
        if (argc == 0) continue;                // empty command: next iteration
        argc = parse_comments(argv);
        if (argc == 0) continue; // empty command after parsing comment #
        argc = parse_background(argv, &background);
        if (argc == 0) continue; // empty command after parsing background &
        argc = parse_redirections(argv,  &file_in, &file_out);
        if (argc == 0) continue; // empty command after parsing redirections
        parse_escape(argv);
        int control = parse_autovars(argc, argv, pid_terminal, pid_fork, wstatus);
        if(control==-1){
          printf("Error en parse autovars\n");
          continue;
        }

        //Comandos internos
        if(strcmp(argv[0], "cd")==0){
          if(argc == 2){
            int controlch =chdir(argv[1]);
            if(controlch==-1){
              perror("chdir");
            }
          }else{
            printf("Error con los argumentos de cd\n");   
          }
          continue;
        }

        if(strcmp(argv[0], "jobs")==0){
          print_job_list(listaProcesos);
          continue;
        }
       if(strcmp(argv[0], "exit")==0){
         if(argc>1){

           errno = 0;
           char *endchar;
           char * str = argv[1];
           int retval = strtol(str, &endchar, 10);

           if(errno == ERANGE){
             printf("Overflow\n");
             continue;
           }else if(*endchar != '\0'){
             printf("Conversion de parte de la cadena\n");
             continue;
           }else{
             exit(retval);
           }
         }else{
           printf("Error con los argumentos de exit\n");
           continue;
         }
       }
        
        pid_fork = fork();
        if(pid_fork == -1){
          perror("fork");
          continue;
        }

        if(pid_fork ==0){
          setpgid(0,0);
          if(!background){
           tcsetpgrp(STDIN_FILENO, pid_fork); 
          }
                    terminal_signals(SIG_DFL);
          execvp(argv[0], argv);
          perror(argv[0]);
          exit(EXIT_FAILURE);
        }else{ 
          if(!background){
            insert_item(listaProcesos, new_job(pid_fork,argv[0], FOREGROUND));
            setpgid(pid_fork,pid_fork);
            tcsetpgrp(STDIN_FILENO,pid_fork);
            pid_wait = waitpid(pid_fork, &wstatus, WUNTRACED);
            tcsetpgrp(STDIN_FILENO,getpid());
            if(-1 == pid_wait){
              perror("waitpid");
              continue;
            }else{
              if(WIFSIGNALED(wstatus)){
                printf("[%d] (%s) Signaled by signal %d\n", pid_wait,argv[0], WTERMSIG(wstatus));
                remove_item(listaProcesos, get_job_bypid(listaProcesos,pid_fork));
              }else if(WIFEXITED(wstatus)){
                printf("[%d] (%s) Exit by signal %d\n", pid_wait, argv[0],WEXITSTATUS(wstatus));
                remove_item(listaProcesos, get_job_bypid(listaProcesos,pid_fork));
              }else if (WIFSTOPPED(wstatus)) {
                    printf("[%d] (%s) Stopped by Signal:  %d\n", pid_wait, argv[0],WSTOPSIG(wstatus));
                    job * aux = get_job_bypid(listaProcesos, pid_fork);
                    aux->state = STOPPED;
              }}
          }else{
            printf("[%d] (%s) Running in background\n", pid_wait, argv[0]);
            insert_item(listaProcesos, new_job(pid_fork, argv[0],BACKGROUND));
          } 
        }


        // the steps are:
        // (1) fork a child process using fork()
        // (2) the child process will invoke execvp()
        // (3) if background == 0, the parent will wait, otherwise
        // (4) Shell shows a status message for processed command 
        // (5) loop ret

    } // end while
    printf("\nBye\n");
    return 0;
}

