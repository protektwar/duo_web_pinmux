#ifndef _DUO_PINMUX_H_
#define _DUO_PINMUX_H_

#include "devmem.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TRUE 1
#define FALSE 0

uint32_t convert_func_to_value(char *pin, char *func);
void list_fun();
char* generate_html_code_pin(char *name, int enabled);
char* print_func(char *name, uint32_t value, int enabled);
int change_pin_function(char *pin, char *func);

#endif
