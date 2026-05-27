#include <stdio.h>
#include <stdlib.h>

int main(){
  int status;
  int pid = fork();
  if(pid!=0){
    wait(&status);
  }else{
    while(1){
      printf("A");
      fflush(NULL);
      sleep(1);
    }
  }
}
