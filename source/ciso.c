/*
    This file is part of Ciso.

    Ciso is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Ciso is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


    Copyright 2005 BOOSTER
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "ciso.h"
#include "saves.h"

/****************************************************************************
	compress ISO to CSO
****************************************************************************/

static uint64_t check_file_size(FILE *fp, CISO_Hdr* ciso, int* ciso_total_block)
{
	uint64_t pos;

	if( fseek(fp,0,SEEK_END) < 0)
		return -1;
	pos = ftell(fp);
	if(pos==-1) return pos;

	/* init ciso header */
	memset(ciso, 0, sizeof(CISO_Hdr));
	memcpy(ciso->magic, "CISO", 4);

	ciso->ver         = 0x01;
	ciso->header_size = sizeof(CISO_Hdr);
	ciso->block_size  = 0x800; /* ISO9660 one of sector */
	ciso->total_bytes = pos;

	*ciso_total_block = pos / ciso->block_size;

	fseek(fp,0,SEEK_SET);

	return pos;
}

/****************************************************************************
	decompress CSO to ISO
****************************************************************************/
int convert_cso2iso(const char *fname_in)
{
	char fname_out[256];
	FILE *fin,*fout;
	z_stream z;
	CISO_Hdr ciso;
	int ciso_total_block;

	uint32_t *index_buf = NULL;
	uint8_t *block_buf1 = NULL;
	uint8_t *block_buf2 = NULL;

	uint64_t file_size, read_pos, read_size;
	uint32_t index;
	int index_size, plain, ret = 0;

	strncpy(fname_out, fname_in, sizeof(fname_out));
	strcpy(strrchr(fname_out, '.'), ".ISO");

	if ((fin = fopen(fname_in, "rb")) == NULL)
	{
		LOG("Can't open %s\n", fname_in);
		return 0;
	}
	if ((fout = fopen(fname_out, "wb")) == NULL)
	{
		LOG("Can't create %s\n", fname_out);
		return 0;
	}

	/* read header */
	if( fread(&ciso, 1, sizeof(CISO_Hdr), fin) != sizeof(CISO_Hdr) )
	{
		LOG("file read error\n");
		return 0;
	}

	/* check header */
	if(
		ciso.magic[0] != 'C' ||
		ciso.magic[1] != 'I' ||
		ciso.magic[2] != 'S' ||
		ciso.magic[3] != 'O' ||
		ciso.block_size ==0  ||
		ciso.total_bytes == 0
	)
	{
		LOG("ciso file format error\n");
		return 0;
	}
	 
	ciso_total_block = ciso.total_bytes / ciso.block_size;

	/* allocate index block */
	index_size = (ciso_total_block + 1 ) * sizeof(uint32_t);
	index_buf  = malloc(index_size);
	block_buf1 = malloc(ciso.block_size);
	block_buf2 = malloc(ciso.block_size*2);

	if( !index_buf || !block_buf1 || !block_buf2 )
	{
		LOG("Can't allocate memory\n");
		return 0;
	}
	memset(index_buf,0,index_size);

	/* read index block */
	if( fread(index_buf, 1, index_size, fin) != index_size )
	{
		LOG("file read error\n");
		goto end_iso;
	}

	/* show info */
	LOG("Decompress '%s' to '%s'\n",fname_in,fname_out);
	LOG("Total File Size %lld bytes\n",ciso.total_bytes);
	LOG("block size      %d  bytes\n",ciso.block_size);
	LOG("total blocks    %d  blocks\n",ciso_total_block);
	LOG("index align     %d\n",1<<ciso.align);

	/* init zlib */
	memset(&z,0,sizeof(z_stream));

	/* decompress data */
	init_progress_bar("Decompressing...");

	for(int block = 0;block < ciso_total_block ; block++)
	{
		if (block % 0x100 == 0)
		{
			update_progress_bar(block, ciso_total_block, "Decompressing...");
		}

		if (inflateInit2(&z,-15) != Z_OK)
		{
			LOG("deflateInit : %s\n", (z.msg) ? z.msg : "error");
			goto end_iso;
		}

		/* check index */
		index  = index_buf[block];
		plain  = index & 0x80000000;
		index  &= 0x7fffffff;
		read_pos = index << (ciso.align);
		if(plain)
		{
			read_size = ciso.block_size;
		}
		else
		{
			read_size = ((index_buf[block+1] & 0x7fffffff)-index) << (ciso.align);
		}
		fseek(fin,read_pos,SEEK_SET);

		z.avail_in  = fread(block_buf2, 1, read_size , fin);
		if(z.avail_in != read_size)
		{
			LOG("block=%d : read error\n",block);
			goto end_iso;
		}

		if(plain)
		{
			memcpy(block_buf1,block_buf2,read_size);
			z.total_out = read_size;
		}
		else
		{
			z.next_out  = block_buf1;
			z.avail_out = ciso.block_size;
			z.next_in   = block_buf2;

			if (inflate(&z, Z_FINISH) != Z_STREAM_END)
			{
				LOG("block %d:inflate : %s\n", block,(z.msg) ? z.msg : "error");
				goto end_iso;
			}
			if(z.total_out != ciso.block_size)
			{
				LOG("block %d : block size error %d != %d\n",block, z.total_out, ciso.block_size);
				goto end_iso;
			}
		}
		/* write decompressed block */
		if(fwrite(block_buf1, 1, z.total_out, fout) != z.total_out)
		{
			LOG("block %d : Write error\n",block);
			goto end_iso;
		}

		/* term zlib */
		if (inflateEnd(&z) != Z_OK)
		{
			LOG("inflateEnd : %s\n", (z.msg) ? z.msg : "error");
			goto end_iso;
		}
	}
	update_progress_bar(ciso_total_block, ciso_total_block, "Done!");
	ret = 1;

end_iso:
	/* close files */
	fclose(fin);
	fclose(fout);

	end_progress_bar();
	LOG("ciso decompress completed\n");

	/* free memory */
	if(index_buf) free(index_buf);
	if(block_buf1) free(block_buf1);
	if(block_buf2) free(block_buf2);

	return ret;
}

/****************************************************************************
	compress ISO
****************************************************************************/
int convert_iso2cso(const char *fname_in)
{
	char fname_out[256];
	FILE *fin,*fout;
	z_stream z;
	CISO_Hdr ciso;
	int ciso_total_block, ret = 0;

	uint32_t *index_buf = NULL;
	uint8_t *block_buf1 = NULL;
	uint8_t *block_buf2 = NULL;

	uint64_t file_size, write_pos;
	int index_size, block;

	strncpy(fname_out, fname_in, sizeof(fname_out));
	strcpy(strrchr(fname_out, '.'), ".CSO");

	if ((fin = fopen(fname_in, "rb")) == NULL)
	{
		LOG("Can't open %s\n", fname_in);
		return 0;
	}
	if ((fout = fopen(fname_out, "wb")) == NULL)
	{
		LOG("Can't create %s\n", fname_out);
		return 0;
	}

	file_size = check_file_size(fin, &ciso, &ciso_total_block);
	if(file_size==(uint64_t)-1LL)
	{
		LOG("Can't get file size\n");
		return 0;
	}

	/* allocate index block */
	index_size = (ciso_total_block + 1 ) * sizeof(uint32_t);
	index_buf  = malloc(index_size);
	block_buf1 = malloc(ciso.block_size);
	block_buf2 = malloc(ciso.block_size);

	if( !index_buf || !block_buf1 || !block_buf2 )
	{
		LOG("Can't allocate memory\n");
		return 0;
	}
	memset(index_buf,0,index_size);

	/* init zlib */
	memset(&z,0,sizeof(z_stream));

	/* show info */
	LOG("Compress '%s' to '%s'\n",fname_in,fname_out);
	LOG("Total File Size %lld bytes\n",ciso.total_bytes);
	LOG("block size      %d  bytes\n",ciso.block_size);
	LOG("index align     %d\n",1<<ciso.align);
	LOG("compress level  %d\n",Z_BEST_COMPRESSION);

	/* write header block */
	fwrite(&ciso,1,sizeof(CISO_Hdr),fout);

	/* dummy write index block */
	fwrite(index_buf,1,index_size,fout);

	write_pos = sizeof(CISO_Hdr) + index_size;

	/* compress data */
	init_progress_bar("Compressing...");

	for(block = 0;block < ciso_total_block ; block++)
	{
		if (block % 0x100 == 0)
		{
			update_progress_bar(block, ciso_total_block, "Compressing...");
		}

		if (deflateInit2(&z, Z_BEST_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
		{
			LOG("deflateInit : %s\n", (z.msg) ? z.msg : "error");
			goto end_cso;
		}

		/* mark offset index */
		index_buf[block] = write_pos>>(ciso.align);

		/* read buffer */
		z.next_out  = block_buf2;
		z.avail_out = ciso.block_size;
		z.next_in   = block_buf1;
		z.avail_in  = fread(block_buf1, 1, ciso.block_size, fin);
		if(z.avail_in != ciso.block_size)
		{
			LOG("block=%d : read error\n",block);
			goto end_cso;
		}

		/* compress block */
		if (deflate(&z, Z_FINISH) != Z_STREAM_END)
		{
			z.total_out = ciso.block_size;
			memcpy(block_buf2,block_buf1,ciso.block_size);
			/* plain block mark */
			index_buf[block] |= 0x80000000;
		}

		/* write compressed block */
		if(fwrite(block_buf2, 1, z.total_out, fout) != z.total_out)
		{
			LOG("block %d : Write error\n",block);
			goto end_cso;
		}

		/* mark next index */
		write_pos += z.total_out;

		/* term zlib */
		if (deflateEnd(&z) != Z_OK)
		{
			LOG("deflateEnd : %s\n", (z.msg) ? z.msg : "error");
			goto end_cso;
		}
	}
	update_progress_bar(ciso_total_block, ciso_total_block, "Done!");
	ret = 1;

	/* last position (total size)*/
	index_buf[block] = write_pos>>(ciso.align);

	/* write header & index block */
	fseek(fout,sizeof(CISO_Hdr),SEEK_SET);
	fwrite(index_buf,1,index_size,fout);
	fseek(fout,0,SEEK_END);

end_cso:
	/* close files */
	fclose(fin);
	fclose(fout);

	end_progress_bar();
	LOG("ciso compress completed , total size = %8d bytes , rate %d%%\n"
		,(int)write_pos,(int)(write_pos*100/ciso.total_bytes));

	/* free memory */
	if(index_buf) free(index_buf);
	if(block_buf1) free(block_buf1);
	if(block_buf2) free(block_buf2);

	return ret;
}

/****************************************************************************
	fprintf(stderr, "Compressed ISO9660 converter Ver.1.02 by BOOSTER\n");
****************************************************************************/
