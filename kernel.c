#include <stdint.h>
#include "system.h"
#include "idt.h"
#include "print.h"


__attribute__((section(".text.main")))
void main() {
    load_idt();

    char *screen = (char*) 0xb8000;
    char *str = "this is from C";
    
    screen += 160;
    print(str, screen);

    screen += 160;
    print(str, screen);

    
    //int fault = 1/0;

    while(1);
}
