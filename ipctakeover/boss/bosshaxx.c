#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

//Build with: gcc -o bosshaxx bosshaxx.c

FILE *fout = NULL;

//50424f532d382e302f303030303030303030303030303030302d303030303030303030303030303030302f30302e302e302d3030582f30303030302f3000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000002c0308343434343535353536363636373737375f1912000000000074b80308b8b8030800000000602a1400f073ffff

char configxml_formatstr[] = {
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<config>\n\
	<targeturl>\n\
		<name>bosshaxx</name>\n\
		<caps>AddRequestHeader</caps>\n\
		<url>%s</url>\n\
		<new_url>%s</new_url>\n\
\n\
		<requestoverride type=\"reqheader\">\n\
			<name>User-Agent</name>\n\
			<new_value format=\"hex\">%s</new_value>\n\
			<new_descriptorword_value>0xbcc</new_descriptorword_value>\n\
		</requestoverride>\n\
	</targeturl>\n\
</config>\n\
"};

//From ctrtool.
void putle32(u8* p, u32 n)
{
	p[0] = n;
	p[1] = n>>8;
	p[2] = n>>16;
	p[3] = n>>24;
}

void buildrop_config(u32 *ropchain)
{
	u32 ropvaddr = 0x0803b7f8-0xa4;
}

void buildrop_http(u32 *ropchain)
{
	u32 ropvaddr = 0x08032c00;
}

int main(int argc, char **argv)
{
	int using_outpath = 0;
	int output_type = -1;
	int argi;
	u32 pos;

	u32 *ropchain = NULL;
	u8 *ropchain8 = NULL;
	char *configout = NULL;
	u32 configout_size = 0x400;
	u32 ropchain_maxsize = 0x1000;

	char *url = NULL, *new_url = NULL;
	char config_hexdata[0x200];

	fout = stdout;

	url = "https://nppl.c.app.nintendowifi.net/p01/policylist/";

	if(argc<3)
	{
		printf("bosshaxx by yellows8.\n");
		printf("Generate the ctr-httpwn config and http data for 3DS BOSS-sysmodule haxx.\n");
		printf("Usage:\n");
		printf("%s <config|http> <version> <options>\n", argv[0]);
		printf("Supported versions: 'v13314'.\n");
		printf("Options:\n");
		printf("--outpath=<filepath> Output path. If not specified stdout will be used instead.\n");
		printf("--url=<url> ctr-httpwn config <url> tag content. Default is the policylist url.\n");
		printf("--new_url=<url> ctr-httpwn config <new_url> tag content. Required with the config type.\n");
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

	for(argi=3; argi<argc; argi++)
	{
		if(strncmp(argv[argi], "--outpath=", 10)==0)
		{
			fout = fopen(&argv[argi][10], "wb");
			if(fout==NULL)
			{
				printf("Failed to open output file: %s\n", &argv[argi][10]);
				return 4;
			}

			using_outpath = 1;
		}
		else if(strncmp(argv[argi], "--url=", 6)==0)
		{
			url = &argv[argi][6];
		}
		else if(strncmp(argv[argi], "--new_url=", 10)==0)
		{
			new_url = &argv[argi][10];
		}
	}

	if(output_type==0 && new_url==NULL)
	{
		printf("The new_url is required.\n");
		return 6;
	}

	if(output_type==0)
	{
		configout = malloc(configout_size);
		if(configout==NULL)
		{
			if(using_outpath)fclose(fout);
			printf("Failed to allocate memory for configout.\n");
			return 5;
		}
	}

	ropchain = malloc(ropchain_maxsize);
	ropchain8 = (u8*)ropchain;
	if(ropchain==NULL)
	{
		if(using_outpath)fclose(fout);
		if(output_type==0)free(configout);
		printf("Failed to allocate memory for ropchain.\n");
		return 5;
	}

	if(output_type==0)buildrop_config(ropchain);
	if(output_type==1)buildrop_http(ropchain);

	putle32(ropchain8, 0x44444444);

	if(output_type==0)
	{
		memset(config_hexdata, 0, sizeof(config_hexdata));

		for(pos=0; pos<0xbc; pos++)sprintf(&config_hexdata[pos*2], "%02x", (unsigned int)ropchain8[pos]);

		snprintf(configout, configout_size-1, configxml_formatstr, url, new_url, config_hexdata);
		fprintf(fout, "%s", configout);
	}

	if(output_type==0)free(configout);
	free(ropchain);

	if(using_outpath)
	{
		fclose(fout);
	}
	else
	{
		fflush(fout);
	}

	return 0;
}

