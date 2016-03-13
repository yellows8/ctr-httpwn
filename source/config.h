#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _targeturl_requestoverridectx {
	u32 next_vaddr;//Address of the next struct in the list in HTTP-sysmodule memory.
	struct _targeturl_requestoverridectx *next;//Src data to copy to next_sharedmemptr.
	struct _targeturl_requestoverridectx *next_sharedmemptr;

	u32 id;//Normally this should be unique, but it can be shared with multiple contexts for having required_id trigger with multiple contexts.
	u32 setid_onmatch;//When non-zero, copy the id value to a field in the targeturlctx when matching request data is found.
	u32 required_id;//When non-zero, this value must match the field referenced above in the targeturlctx in order for the request data to completely match.

	char name[0x40];//Must match the entire input name.
	char value[0x40];//Optional, when set this must match the entire input value.
	char new_value[0x40];
} targeturl_requestoverridectx;

typedef enum {
	TARGETURLCAP_AddRequestHeader = 1<<0, //Override the value-data used for certain headers.
	TARGETURLCAP_AddPostDataAscii = 1<<1, //Override the value-data used for certain form-fields.
	TARGETURLCAP_SendPOSTDataRawTimeout = 2<<0, //NOP SendPOSTDataRawTimeout.
} targeturl_caps;//Bitmask of capabilities, 0 = none(vtable-ptr is left at the original one).

typedef struct _targeturlctx {
	u32 next_vaddr;//Address of the next struct in the list in HTTP-sysmodule memory.
	struct _targeturlctx *next;

	targeturl_caps caps;

	u32 lastmatch_id;//Set to zero when the CreateContext ROP-chain runs with this ctx. This is the targeturlctx field mentioned in requestoverridectx.

	u32 vtableptr;
	u32 *vtable_sharedmemptr;

	u32 reqheader_first_vaddr;//Address of the first struct in the list in HTTP-sysmodule memory for request-headers. Used with AddRequestHeader.
	targeturl_requestoverridectx *reqheader;
	targeturl_requestoverridectx *reqheader_sharedmemptr;

	u32 postform_first_vaddr;//Address of the first struct in the list in HTTP-sysmodule memory for post-forms. Used with AddPostDataAscii.
	targeturl_requestoverridectx *postform;
	targeturl_requestoverridectx *postform_sharedmemptr;

	char url[0x100];//Target URL to compare the CreateContext input URL with. The compare will not include the NUL-terminator in this target URL, hence the check will pass when there's additional chars following the matched URL. This is needed due to the multiple account.nintendo.net URLs that need targeted + handled all the same way.
	char new_url[0x100];//Optional, when set the specified URL will overwrite the URL used with CreateContext, NUL-terminator included.
} targeturlctx;

int config_parse(targeturlctx **first_targeturlctx, char *xml);
void config_freemem(targeturlctx **first_targeturlctx);

#ifdef __cplusplus
}
#endif

