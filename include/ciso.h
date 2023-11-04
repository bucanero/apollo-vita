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

#include <stdint.h>

#ifndef __CISO_H__
#define __CISO_H__
/*
	compressed ISO(9660) header format
*/
typedef struct ciso_header
{
	uint8_t magic[4];			/* +00 : 'C','I','S','O'                 */
	uint32_t header_size;		/* +04 : header size (==0x18)            */
	uint64_t total_bytes;	/* +08 : number of original data size    */
	uint32_t block_size;		/* +10 : number of compressed block size */
	uint8_t ver;				/* +14 : version 01                      */
	uint8_t align;			/* +15 : align of index value            */
	uint8_t rsv_06[2];		/* +16 : reserved                        */
#if 0
// INDEX BLOCK
	uint32_t index[0];			/* +18 : block[0] index                  */
	uint32_t index[1];			/* +1C : block[1] index                  */
             :
             :
	uint32_t index[last];		/* +?? : block[last]                     */
	uint32_t index[last+1];		/* +?? : end of last data point          */
// DATA BLOCK
	uint8_t data[];			/* +?? : compressed or plain sector data */
#endif
}CISO_Hdr;

/*
note:

file_pos_sector[n]  = (index[n]&0x7fffffff) << CISO_H.align
file_size_sector[n] = ( (index[n+1]&0x7fffffff) << CISO_H.align) - file_pos_sector[n]

if(index[n]&0x80000000)
  // read 0x800 without compress
else
  // read file_size_sector[n] bytes and decompress data
*/

#endif

