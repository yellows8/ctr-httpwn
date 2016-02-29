#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <3ds.h>

//The decompression code here is from: https://github.com/smealum/ninjhax2.x/blob/master/app_bootloader/source/takeover.c

u32 getle32(const u8* p)
{
	return (p[0]<<0) | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

u32 lzss_get_decompressed_size(u8* compressed, u32 compressedsize)
{
	u8* footer = compressed + compressedsize - 8;

	u32 originalbottom = getle32(footer+4);

	return originalbottom + compressedsize;
}

int lzss_decompress(u8* compressed, u32 compressedsize, u8* decompressed, u32 decompressedsize)
{
	u8* footer = compressed + compressedsize - 8;
	u32 buffertopandbottom = getle32(footer+0);
	//u32 originalbottom = getle32(footer+4);
	u32 i, j;
	u32 out = decompressedsize;
	u32 index = compressedsize - ((buffertopandbottom>>24)&0xFF);
	u32 segmentoffset;
	u32 segmentsize;
	u8 control;
	u32 stopindex = compressedsize - (buffertopandbottom&0xFFFFFF);

	memset(decompressed, 0, decompressedsize);
	memcpy(decompressed, compressed, compressedsize);

	
	while(index > stopindex)
	{
		control = compressed[--index];
		

		for(i=0; i<8; i++)
		{
			if (index <= stopindex)
				break;

			if (index <= 0)
				break;

			if (out <= 0)
				break;

			if (control & 0x80)
			{
				if (index < 2)
				{
					// fprintf(stderr, "Error, compression out of bounds\n");
					goto clean;
				}

				index -= 2;

				segmentoffset = compressed[index] | (compressed[index+1]<<8);
				segmentsize = ((segmentoffset >> 12)&15)+3;
				segmentoffset &= 0x0FFF;
				segmentoffset += 2;

				
				if (out < segmentsize)
				{
					// fprintf(stderr, "Error, compression out of bounds\n");
					goto clean;
				}

				for(j=0; j<segmentsize; j++)
				{
					u8 data;
					
					if (out+segmentoffset >= decompressedsize)
					{
						// fprintf(stderr, "Error, compression out of bounds\n");
						goto clean;
					}

					data  = decompressed[out+segmentoffset];
					decompressed[--out] = data;
				}
			}
			else
			{
				if (out < 1)
				{
					// fprintf(stderr, "Error, compression out of bounds\n");
					goto clean;
				}
				decompressed[--out] = compressed[--index];
			}

			control <<= 1;
		}
	}

	return 0;
	
	clean:
	return -1;
}

Result loadcodebin(u64 programid, FS_MediaType mediatype, u8 **codebin_buf, u32 *codebin_size)
{
	Result ret = 0;
	Handle filehandle = 0;
	u64 filesize = 0;
	u32 transfersize = 0;
	u8 *readbuf;
	u8 *decombuf;
	u32 decomsize = 0;

	u32 archive_lowpath_data[0x10>>2];
	u32 file_lowpath_data[0x14>>2];

	FS_Archive archive;
	FS_Path fileLowPath;

	memset(archive_lowpath_data, 0, sizeof(archive_lowpath_data));
	memset(file_lowpath_data, 0, sizeof(file_lowpath_data));

	archive.id = 0x2345678a;
	archive.lowPath.type = PATH_BINARY;
	archive.lowPath.size = 0x10;
	archive.lowPath.data = archive_lowpath_data;

	fileLowPath.type = PATH_BINARY;
	fileLowPath.size = 0x14;
	fileLowPath.data = file_lowpath_data;

	archive_lowpath_data[0] = (u32)programid;
	archive_lowpath_data[1] = (u32)(programid>>32);
	archive_lowpath_data[2] = mediatype;

	file_lowpath_data[2] = 0x2;
	file_lowpath_data[3] = 0x646f632e;
	file_lowpath_data[4] = 0x65;

	ret = FSUSER_OpenFileDirectly(&filehandle, archive, fileLowPath, FS_OPEN_READ, 0x0);
	if(R_FAILED(ret))return ret;

	ret = FSFILE_GetSize(filehandle, &filesize);
	if(R_FAILED(ret))
	{
		FSFILE_Close(filehandle);
		return ret;
	}

	if(filesize & (1<<31))
	{
		FSFILE_Close(filehandle);
		return -2;
	}

	readbuf = malloc(filesize);
	if(readbuf==NULL)
	{
		FSFILE_Close(filehandle);
		return -3;
	}

	memset(readbuf, 0, filesize);

	ret = FSFILE_Read(filehandle, &transfersize, 0, readbuf, filesize);
	if(R_FAILED(ret) || transfersize!=filesize)
	{
		free(readbuf);
		FSFILE_Close(filehandle);
		if(transfersize!=filesize)return -4;
		return ret;
	}

	FSFILE_Close(filehandle);

	decomsize = lzss_get_decompressed_size(readbuf, filesize);
	if(decomsize & (1<<31))
	{
		free(readbuf);
		return -5;
	}

	decombuf = malloc(decomsize);
	if(decombuf==NULL)
	{
		free(readbuf);
		return -6;
	}
	memset(decombuf, 0, decomsize);

	ret = lzss_decompress(readbuf, filesize, decombuf, decomsize);

	free(readbuf);

	if(ret==0)
	{
		*codebin_buf = decombuf;
		*codebin_size = decomsize;
	}
	else
	{
		free(decombuf);
	}

	return ret;
}

