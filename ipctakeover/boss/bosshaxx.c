#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

//Build with: gcc -o bosshaxx bosshaxx.c

FILE *fout = NULL;

//From ctrtool.
void putle32(u8* p, u32 n)
{
	p[0] = n;
	p[1] = n>>8;
	p[2] = n>>16;
	p[3] = n>>24;
}

int main(int argc, char **argv)
{
	int output_type = -1;
	u8 ropchain[4];

	if(argc<3)
	{
		printf("bosshaxx by yellows8.\n");
		printf("Generate the ctr-httpwn config and http data for 3DS BOSS-sysmodule haxx.\n");
		printf("Usage:\n");
		printf("%s <config|http> <version> {outfilepath}\n", argv[0]);
		printf("If outfilepath isn't specified, stdout will be used instead.\n");
		printf("Supported versions: 'v13314'.\n");
		return 1;
	}

	if(strcmp(argv[1], "config")==0)
	{
		output_type = 0;
	}
	else if(strcmp(argv[1], "http")==0)//Can be used with policylist or anything requested with HTTP GET.
	{
		output_type = 1;
	}
	else
	{
		printf("Invalid output_type: %s\n", argv[1]);
		return 2;
	}

	if(strcmp(argv[2], "v13314"))
	{
		printf("Invalid version: %s\n", argv[2]);
		return 3;
	}

	fout = stdout;
	if(argc>=4)
	{
		fout = fopen(argv[3], "wb");
		if(fout==NULL)
		{
			printf("Failed to open output file: %s\n", argv[3]);
			return 4;
		}
	}

	putle32(ropchain, 0x44444444);
	fwrite(ropchain, 1, 4, fout);

	if(argc>=4)
	{
		fclose(fout);
	}
	else
	{
		fflush(fout);
	}

	return 0;
}

