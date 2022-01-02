#include "debug.h"

void dbg_info(char *msg)
{
	printf("[INFO] \"%s\"\n", msg);
}

void dbg_log(char *msg)
{
	printf("[LOG] \"%s\"\n", msg);
}

void dbg_warn(char *msg)
{
	printf("[WARN] \"%s\"\n", msg);
}

void dbg_error(char *msg)
{
	printf("[ERROR] \"%s\"\n", msg);
	exit(-1);
}