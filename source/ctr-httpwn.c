#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <3ds.h>

#include "cmpblock_bin.h"

extern u32 *__httpc_sharedmem_addr;

void displaymessage_waitbutton()
{
	printf("\nPress the A button to continue.\n");
	while(1)
	{
		gspWaitForVBlank();
		hidScanInput();
		if(hidKeysDown() & KEY_A)break;
	}
}

//This searches physmem for the page which starts with the data stored in cmpblock_bin. The first byte in cmpblock is XORed with 0x01 to avoid detecting the cmpblock in physmem.
Result locate_sharedmem_linearaddr(u32 **linearaddr)
{
	u8 *tmpbuf;
	u32 chunksize = 0x100000;
	u32 linearpos, bufpos, size;
	u32 i;
	u32 xorval;
	int found = 0;

	*linearaddr = NULL;

	tmpbuf = linearAlloc(chunksize);
	if(tmpbuf==NULL)
	{
		printf("Failed to allocate mem for tmpbuf.\n");
		return -1;
	}

	size = osGetMemRegionSize(MEMREGION_APPLICATION);

	for(linearpos=0; linearpos<size; linearpos+= chunksize)
	{
		*linearaddr = (u32*)(0x30000000+linearpos);

		memset(tmpbuf, 0, chunksize);
		GSPGPU_FlushDataCache(tmpbuf, chunksize);

		GX_TextureCopy(*linearaddr, 0, (u32*)tmpbuf, 0, chunksize, 0x8);
		gspWaitForPPF();

		for(bufpos=0; bufpos<chunksize; bufpos+= 0x1000)
		{
			found = 1;

			for(i=0; i<cmpblock_bin_size; i++)
			{
				xorval = 0;
				if(i==0)xorval = 1;

				if(tmpbuf[bufpos + i] != (cmpblock_bin[i] ^ xorval))
				{
					found = 0;
					break;
				}
			}

			if(found)
			{
				*linearaddr = (u32*)(0x30000000+linearpos+bufpos);
				break;
			}
		}
		if(found)break;
	}

	linearFree(tmpbuf);

	if(!found)return -1;

	return 0;
}

Result http_haxx(char *requrl)
{
	Result ret=0;
	httpcContext context;
	u32 *linearaddr = NULL;

	ret = httpcOpenContext(&context, HTTPC_METHOD_POST, requrl, 1);
	if(ret!=0)return ret;

	ret = httpcAddPostDataAscii(&context, "form_name", "form_value");
	if(ret!=0)
	{
		httpcCloseContext(&context);
		return ret;
	}

	//Locate the physmem for the httpc sharedmem. With the current cmpblock, there can only be one POST struct that was ever written into sharedmem, with the name/value from above.
	printf("Searching for the httpc sharedmem in physmem...\n");
	ret = locate_sharedmem_linearaddr(&linearaddr);
	if(ret!=0)
	{
		printf("Failed to locate the sharedmem in physmem.\n");
		httpcCloseContext(&context);
		return ret;
	}

	printf("Successfully located the linearaddr for sharedmem: 0x%08x.\n", (unsigned int)linearaddr);

	httpcCloseContext(&context);

	return 0;
}

Result httpwn_setup()
{
	Result ret = 0;
	u64 http_sysmodule_titleid = 0x0004013000002902ULL;
	AM_TitleEntry title_entry;

	ret = amInit();
	if(ret!=0)
	{
		printf("Failed to initialize AM: 0x%08x.\n", (unsigned int)ret);
		if(ret==0xd8e06406)
		{
			printf("The AM service is inaccessible. With the *hax payloads this should never happen. This is normal with plain ninjhax v1.x: this app isn't usable from ninjhax v1.x without any further hax.\n");
		}
		return ret;
	}

	ret = AM_ListTitles(0, 1, &http_sysmodule_titleid, &title_entry);
	amExit();
	if(ret!=0)
	{
		printf("Failed to get the HTTP sysmodule title-version: 0x%08x.\n", (unsigned int)ret);
		return ret;
	}

	if(title_entry.version != 13318)
	{
		printf("The installed HTTP sysmodule version(v%u) is not supported. The sysmodule version must be the version from system-version >=9.6.0-X.\n", title_entry.version);
		return -1;
	}

	ret = httpcInit(0x1000);
	if(ret!=0)
	{
		printf("Failed to initialize HTTPC: 0x%08x.\n", (unsigned int)ret);
		if(ret==0xd8e06406)
		{
			printf("The HTTPC service is inaccessible. With the *hax payload this may happen if the process this app is running under doesn't have access to that service. Please try rebooting the system, boot *hax payload, then directly launch the app.\n");
		}

		return ret;
	}

	printf("Preparing the haxx...\n");
	ret = http_haxx("http://localhost/");//URL doesn't matter much since this won't actually be requested over the network.
	httpcExit();
	if(ret!=0)
	{
		printf("HTTP request failed: 0x%08x.\n", (unsigned int)ret);
		return ret;
	}

	return ret;
}

int main(int argc, char **argv)
{
	Result ret = 0;

	// Initialize services
	gfxInitDefault();

	consoleInit(GFX_TOP, NULL);

	printf("ctr-httpwn %s by yellows8.\n", VERSION);

	ret = httpwn_setup();

	if(ret!=0)printf("An error occured. If this is an actual issue not related to user failure, please report this to here if it persists(or comment on an already existing issue if needed), with a screenshot: https://github.com/yellows8/ctr-httpwn /issues\n");

	printf("Press the START button to exit.\n");
	// Main loop
	while (aptMainLoop())
	{
		gspWaitForVBlank();
		hidScanInput();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu
	}

	// Exit services
	gfxExit();
	return 0;
}

