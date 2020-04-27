#include <stdlib.h>
#include "test.h"

.globl main

main:
	mrs r0,cpsr 
	blx param1
	bl _exit


