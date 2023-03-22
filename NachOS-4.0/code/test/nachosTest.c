#include "syscall.h"
int main(){
    int a;
    a = Open("kh.txt");
    Close(a);
    Halt();
}