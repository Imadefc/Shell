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
// Nombre y apellidos: Imad El Founti Chaib
// standard headers
#include <stdio.h>          // printf, stderr, perror, fprintf
#include <stdlib.h>         // malloc, free
//#include <malloc.h>
#include <string.h>         // strcmp
#include <fcntl.h>          // open
#include <unistd.h>         // fork, execvp, tcgetpgrp, dup2, close
#include <termios.h>
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
list_head_t * listaProcesos; //lista de procesos
struct termios shell_modos; //variable conf de shell
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
//handler para controlar las señales sigchild de los procesos 
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
              if(aux==NULL)continue;
              if(WIFSIGNALED(wstatus)){
                printf("[%d] (%s) Signaled by signal %d\n",pid_wait,aux->command, WTERMSIG(wstatus));
                if(aux->state == RESPAWN){
                  int pid_fork = fork();
                  if(pid_fork == -1){
                    perror("fork");
                    continue;
                  }else  if(pid_fork ==0){//HIJO
                    setpgid(0,0);
                    terminal_signals(SIG_DFL);
                    mask_signal(SIGCHLD, SIG_BLOCK); // Desbloqueamos SIGCHLD antes de ejecutar el comando
                    execvp(aux->command, aux->argv);
                    perror(aux->command);
                    exit(255);
                    mask_signal(SIGCHLD, SIG_UNBLOCK); // Bloqueamos SIGCHLD después de ejecutar el comando
                  }else{ //PADRE
                    job* new = new_job(pid_fork, aux->command, RESPAWN);
                    insert_item(listaProcesos, new);
                  }
                }
                remove_item(listaProcesos,get_job_bypid(listaProcesos,pid_wait));
              }
              if(WIFEXITED(wstatus)){
                if(WEXITSTATUS(wstatus)==255){
                  remove_item(listaProcesos,get_job_bypid(listaProcesos,pid_wait));
                  continue;
                };
                
                printf("[%d] (%s) Terminated by signal %d\n", pid_wait,aux->command,WEXITSTATUS(wstatus));
                if(aux->state == RESPAWN){
                  int pid_fork = fork();
                  if(pid_fork == -1){
                    perror("fork");
                    continue;
                  }else  if(pid_fork ==0){//HIJO
                    setpgid(0,0);
                    terminal_signals(SIG_DFL);
                    mask_signal(SIGCHLD, SIG_BLOCK); // Desbloqueamos SIGCHLD antes de ejecutar el comando
                    execvp(aux->command, aux->argv);
                    perror(aux->command);
                    exit(EXIT_FAILURE);
                    mask_signal(SIGCHLD, SIG_UNBLOCK); // Bloqueamos SIGCHLD después de ejecutar el comando
                  }else{ //PADRE
                    job* new = new_job(pid_fork, aux->command, RESPAWN);
                    insert_item(listaProcesos, new);
                  }
                }
                
                
                remove_item(listaProcesos,get_job_bypid(listaProcesos,pid_wait));
                mask_signal(SIGCHLD, SIG_UNBLOCK);
              }
              if(WIFSTOPPED(wstatus)) {
                printf("[%d] Stopped by Signal:  %d\n", pid_wait,WSTOPSIG(wstatus));
                aux->state = STOPPED;
                tcgetattr(STDIN_FILENO,&(aux->modes));
              }
              if(WIFCONTINUED(wstatus)){
                   printf("[%d] Continued .\n", pid_wait);
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
    int custom_mask =0;
    int indiceMask =0;
    sigset_t custom_mask_set;
    
    // probably useful variables:
    int background;             // equals 1 if a command is followed by '&'
    int pid_fork, pid_wait;     // pid for created and waited process
    int wstatus;           // status returned by waitpid
    int respawn;
    listaProcesos = new_list("Jobs");
    char *file_in=NULL;
    char *file_out=NULL;   // for redirections
    terminal_signals(SIG_IGN);
    tcgetattr(STDIN_FILENO,&shell_modos);
    int pid_terminal = getpid();
    signal(SIGCHLD, myHandler); //manejador de la señal sigchild
    while (1) {
        free_argv(argv);
        sigemptyset(&custom_mask_set);
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
        argc= parse_respawn(argv,  &respawn);
        if(argc==0)continue;
        parse_autovars(argc, argv, pid_terminal, pid_fork, wstatus);// parse de las variables $$ $! $?
        argc = parse_redirections(argv,  &file_in, &file_out);
        if (argc == 0) continue; // empty command after parsing redirections
        parse_escape(argv);
        

        //Comandos internos
        if(strcmp(argv[0], "cd")==0){
          char *dest =NULL;
          if(argc==1)dest=getenv("HOME"); //sin argumentos va a home
          else if(argc == 2)dest=argv[1]; //Coge como argumento el siguiente
          else{//si argc>2 demasiados argumentos y se va
            printf("Too many arguments.\n");
          }
          if(dest!=NULL){
            if(chdir(dest)==-1){
              perror("chdir");
            }
          }
          continue;
        }
        //comando para imprimir los jobs almacenados en la lista
        if(strcmp(argv[0], "jobs")==0){
          mask_signal(SIGCHLD, SIG_BLOCK);
          print_job_list(listaProcesos);
          mask_signal(SIGCHLD, SIG_UNBLOCK);
          continue;
        }/*
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
       }*/



       if(strcmp(argv[0], "mask")==0){
        int error =0;
          if(argc>=3){
            int i =1;
            while(i<argc && strcmp(argv[i],"-c")!=0){
              int numero = atoi(argv[i]);
              if(numero == 0 && strcmp(argv[i],"0")!=0 || numero<0 ){
                error =1;
                break;
              }else{
                custom_mask = 1;
                sigaddset(&custom_mask_set, numero);
              }
              i++;
            }
            if(i+1>=argc || error==1){
              printf("mask: error de sintaxis\n");
              continue;
            }else{
              int iniciocomando = i+1;
              for (int j = 0; j < iniciocomando; j++){
                free(argv[j]);
              }
              int k=0;
              for (int  i = 0; i < iniciocomando; i++)
              {
                argv[i] = NULL;
              }
              while(argv[iniciocomando+k]!=NULL){
                argv[k] = argv[iniciocomando+k];
                k++;
              }
              
              
              argc = argc - iniciocomando;
              
              
            }

          }else{
            printf("mask: error de sintaxis\n");
           continue;
          }
         }

       if(respawn){
         pid_fork = fork();
         if(pid_fork == -1){
          perror("fork");
          continue;
         }else  if(pid_fork ==0){//HIJO
          setpgid(0,0);
          sigprocmask(SIG_BLOCK, &custom_mask_set, NULL);
          terminal_signals(SIG_DFL);
          mask_signal(SIGCHLD, SIG_BLOCK); // Desbloqueamos SIGCHLD antes de ejecutar el comando
          execvp(argv[0], argv);
          perror(argv[0]);
          exit(255);
          mask_signal(SIGCHLD, SIG_UNBLOCK);
          continue; // Bloqueamos SIGCHLD después de ejecutar el comando
         }else{
          job* new = new_job(pid_fork, argv[0], RESPAWN);
          insert_item(listaProcesos, new);
         }
          
        continue;
       }


        //Comando fg para poner en primer plano tareas en segundo plan
        // y tareas suspendidas
        if(strcmp(argv[0], "fg")==0){
          
            mask_signal(SIGCHLD, SIG_BLOCK);
            int pos = argc>=2 ?atoi(argv[1]) : 1;
            if(pos==0 ){
              printf("Segundo argumento invalido: %s.\n",argv[1]);
              mask_signal(SIGCHLD, SIG_UNBLOCK);
              continue;
            }
            job * elegido = get_item_bypos(listaProcesos, pos);
            if(elegido==NULL){
              mask_signal(SIGCHLD,SIG_UNBLOCK);
              continue;
            }
            if(elegido!=NULL){
              
              pid_t fgpgid = elegido->pgid;
              char * fgcommand = strdup(elegido->command);
              struct termios configProceso = elegido->modes;
              remove_item(listaProcesos, elegido);
              insert_item(listaProcesos,new_job(fgpgid,fgcommand,FOREGROUND));
              free(fgcommand);
              elegido = get_item_bypos(listaProcesos,1);
              elegido->modes = configProceso;

              printf("[%d] (%s) Running in FOREGROUND\n", fgpgid, elegido->command);//mostrar el mensaje
              tcsetpgrp(STDIN_FILENO, fgpgid);//le damos el control de la terminal al grupo del proceso
              tcsetattr(STDIN_FILENO,TCSANOW, &(elegido->modes));//aplicamos conf de trabajo
              kill(-fgpgid, SIGCONT); //enviamos señal para que cambie de estado


              pid_wait = waitpid(fgpgid, &wstatus, WUNTRACED);
              tcsetpgrp(STDIN_FILENO,pid_terminal);

              if(pid_wait != -1) {
                        if (WIFSIGNALED(wstatus)) {
                            printf("[%d] (%s) Signaled by signal %d\n", pid_wait, elegido->command, WTERMSIG(wstatus));
                            remove_item(listaProcesos, elegido);
                        } else if (WIFEXITED(wstatus)) {
                            printf("[%d] (%s) Exit by signal %d\n", pid_wait, elegido->command, WEXITSTATUS(wstatus));
                            remove_item(listaProcesos, elegido);
                        } else if (WIFSTOPPED(wstatus)) {
                            printf("[%d] (%s) Stopped by Signal: %d\n", pid_wait, elegido->command, WSTOPSIG(wstatus));
                            elegido->state = STOPPED;
                            tcgetattr(STDIN_FILENO,&(elegido->modes));
                        }
                    } else {
                        perror("waitpid en fg");
                    }   
              tcsetattr(STDIN_FILENO,TCSANOW,&shell_modos);
            }
            mask_signal(SIGCHLD,SIG_UNBLOCK);


            continue;
        }
        //Commando interno bg .Poner a ejecutar en segundo plano una tarea suspendida
        if(strcmp(argv[0],"bg")==0){
            int pos = argc>=2 ? atoi(argv[1]) : 1;
            if(pos==0 ){
              printf("Error con el segundo argumento : %s\n",argv[1]);
              continue;
            }
            mask_signal(SIGCHLD, SIG_BLOCK);

            job * elegido = get_item_bypos(listaProcesos,pos);
            if (elegido==NULL){
              mask_signal(SIGCHLD,SIG_UNBLOCK);
              continue;
            };
            if(elegido!=NULL){
              if(elegido->state!=BACKGROUND){
                kill(-(elegido->pgid), SIGCONT);
                elegido->state= BACKGROUND;
                printf("[%i] (%s) Running in BACKGROUND\n", elegido->pgid, elegido->command);

              }else{
                printf("[%i] (%s) Already in BACKGROUND\n", elegido->pgid, elegido->command);
              }

            }else{
              printf("No hay ninguna tarea con indice %i\n",pos);
            }

            mask_signal(SIGCHLD,SIG_UNBLOCK);

             continue;
        }


       
        
        pid_fork = fork();//creacion del hijo
        if(pid_fork == -1){
          perror("fork");
          continue;
        }

        if(pid_fork ==0){//HIJO
          if(custom_mask){
            sigprocmask(SIG_BLOCK, &custom_mask_set, NULL);
          }
          setpgid(0,0);
          if(!background){
           tcsetpgrp(STDIN_FILENO, getpid()); 
          }
          terminal_signals(SIG_DFL);
          //control de la entrada 
          if(file_in!=NULL){
             int fd_in = open(file_in,O_RDONLY);
              if(fd_in==1){
                perror("Error abriendo el fichero de entrada");
                exit(EXIT_FAILURE);
              }

              if(dup2(fd_in, STDIN_FILENO)==-1){
                perror("Error dup2 para entrada");
              }
              close(fd_in);

          } 

          if(file_out !=NULL){
            int fd_out = open(file_out, O_WRONLY| O_CREAT | O_TRUNC, 0644);
            if(fd_out==-1){
              perror("Error abriendo el fichero de salida");
              exit(EXIT_FAILURE);
            }
            if(dup2(fd_out,STDOUT_FILENO)==-1){
              perror("Error en dup2 para salida");
              exit(EXIT_FAILURE);
            }
            close(fd_out);
          }
         
          execvp(argv[0], argv);
          perror(argv[0]);
          exit(EXIT_FAILURE);
        }else{

          //PADRE
          if(!background){                                                    //Primer plano
            insert_item(listaProcesos, new_job(pid_fork,argv[0], FOREGROUND));//insertar en cola
            setpgid(pid_fork,pid_fork);                                       //creacion del grupo en carrera
            tcsetpgrp(STDIN_FILENO,pid_fork);
            //asignacion de terminal
            mask_signal(SIGCHLD,SIG_BLOCK);
            pid_wait = waitpid(pid_fork, &wstatus, WUNTRACED);                //Esperamos hijo
            tcsetpgrp(STDIN_FILENO,getpid());
            if(-1 == pid_wait){
              perror("waitpid");
              continue;
            }else{
              mask_signal(SIGCHLD, SIG_BLOCK);
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
                    tcgetattr(STDIN_FILENO,&(aux->modes));
              }}
              tcsetattr(STDIN_FILENO,TCSANOW,&shell_modos);
              mask_signal(SIGCHLD, SIG_UNBLOCK);
          }else{
            mask_signal(SIGCHLD, SIG_BLOCK);
            printf("[%d] (%s) Running in background\n", pid_fork, argv[0]);
            insert_item(listaProcesos, new_job(pid_fork, argv[0],BACKGROUND));
            mask_signal(SIGCHLD, SIG_UNBLOCK);
          } 
        
        }


        // the steps are:
        // (1) fork a child process using fork()
        // (2) the child process will invoke execvp()
        // (3) if background == 0, the parent will wait, otherwise
        // (4) Shell shows a status message for processed command 
        // (5) loop ret

    } // end while
    tcsetattr(STDIN_FILENO,TCSANOW,&shell_modos);
    printf("\nBye\n");
    free(argv);
    traverse_list(listaProcesos, (void*)free_job);
    free(listaProcesos);
    return 0;
}

