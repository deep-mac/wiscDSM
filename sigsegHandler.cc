#include<stdio.h>
#include<iostream>
#include<signal.h>


struct sigaction *sa_default;

void faultHandler(int sig, siginfo_t *info, void *ucontext){
    printf("faultHandler triggered\n");
    //exit(1);
    if (sa_default->sa_flags & SA_SIGINFO) {
        (*(sa_default->sa_sigaction))(sig, info, ucontext);
    }
    else {
        printf("Calling defualt handler\n");
        (*(sa_default->sa_handler))(sig);
    }
    //signal(sig, SIG_DFL);
    //raise(sig);

}

int main(){
   
    struct sigaction sa;
    sa_default = (struct sigaction*)malloc(sizeof(struct sigaction));
    sa.sa_sigaction = faultHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;

    sigemptyset(&sa_default->sa_mask);
    sa_default->sa_handler = SIG_DFL;
    sa_default->sa_flags = 0;

    if (sigaction(SIGSEGV, &sa, sa_default)){
        perror("sigaction");
        exit(1);
    }

    sa_default->sa_handler = SIG_DFL;
    sa_default->sa_flags = 0;
       
    printf("before declaring \n");
    int *p;
    p = (int*)malloc(sizeof(int));
    printf("Value before assignment = %d\n", *p);
    *p = 1;
    printf("Value after assignment = %d\n", *p);
    free(p);

    return 0;
}

