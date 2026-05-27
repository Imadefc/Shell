/* 
 
File:   termios.c
Author: corbera*
Created on 20 de noviembre de 2008, 12:30*/
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
struct termios conf;
struct termios conf_new;
int shell_terminal; // descriptor de fichero del terminal
int main(int argc, char** argv) {
    shell_terminal = STDIN_FILENO;
    /* leemos la configuracion actual */
    tcgetattr(shell_terminal, &conf);
    conf_new = conf;

    /* configuramos */
    conf_new.c_lflag &= (~ICANON);
    conf_new.c_lflag &= (~ECHO);
    //conf_new.c_lflag &= (~ISIG);
    conf_new.c_cc[VTIME] = 0;
    conf_new.c_cc[VMIN] = 1;

    /* guardamos la configuracion */
    tcsetattr(shell_terminal, TCSANOW, &conf_new);

    while (1) {
        /* leemos el caracter */
        char c = getc(stdin);
        printf("%c",c);
        if (c == 'Q' || c==EOF)
            break;
    }

    /* restauramos la configuracion */
    tcsetattr(shell_terminal, TCSANOW, &conf);
    return (EXIT_SUCCESS);
}
