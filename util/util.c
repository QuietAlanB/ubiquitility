#include "util.h"

size_t get_file_len(char *filepath)
{
	FILE					*fp;
	size_t					file_len;
	
	fp = fopen(filepath, "rb");
	
	if (fp == NULL)
		dbg_error("could not open file");
		
	fseek(fp, 0, SEEK_END);
	
	file_len = ftell(fp);

	fseek(fp, 0, SEEK_SET);
	
	fclose(fp);

	return file_len;
}

void read_file(char *dest, char *filepath)
{	
	FILE					*fp;

	fp = fopen(filepath, "rb");
	
	if (fp == NULL)
		dbg_error("could not open file");
		
	fread(dest, 1, get_file_len(filepath), fp);

	fclose(fp);
}

uint32_t clamp_uint(uint32_t val, uint32_t min, uint32_t max)
{
	if (val < min)
		return min;
	else if (val > max)
		return max;

	return val;
}