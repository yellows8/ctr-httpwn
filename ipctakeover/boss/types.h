#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>

typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef unsigned long long	u64;

typedef signed char			s8;
typedef signed short		s16;
typedef signed int			s32;
typedef signed long long	s64;

enum flags
{
	ExtractFlag = (1<<0),
	InfoFlag = (1<<1),
	PlainFlag = (1<<2),
	VerboseFlag = (1<<3),
	VerifyFlag = (1<<4),
	RawFlag = (1<<5),
	ShowKeysFlag = (1<<6)
};



enum validstate
{
	Unchecked = 0,
	Good = 1,
	Fail = 2,
};

#endif
