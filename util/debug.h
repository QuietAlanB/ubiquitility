#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>

void dbg_info(char *msg);

void dbg_log(char *msg);

void dbg_warn(char *msg);

void dbg_error(char *msg);

#endif