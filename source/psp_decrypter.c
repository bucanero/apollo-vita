// PSP SaveData En/Decrypter jpcsp backend
// https://github.com/cielavenir/psp-savedata-endecrypter
/*
 This file is part of jpcsp.

 Jpcsp is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Jpcsp is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Jpcsp.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <apollo.h>
#include <dbglogger.h>
#include "kirk_engine.h"

#define LOG dbglogger_log
#define read16(X) *(uint16_t*)(X)
#define read32(X) *(uint32_t*)(X)
#define arraycopy(src,srcPos,dest,destPos,len) memmove((dest)+(destPos),(src)+(srcPos),(len))

static int sdHashKey1[] = {0x40, 0xE6, 0x53, 0x3F, 0x05, 0x11, 0x3A, 0x4E, 0xA1, 0x4B, 0xDA, 0xD6, 0x72, 0x7C, 0x53, 0x4C};
static int sdHashKey2[] = {0xFA, 0xAA, 0x50, 0xEC, 0x2F, 0xDE, 0x54, 0x93, 0xAD, 0x14, 0xB2, 0xCE, 0xA5, 0x30, 0x05, 0xDF};
static int sdHashKey3[] = {0x36, 0xA5, 0x3E, 0xAC, 0xC5, 0x26, 0x9E, 0xA3, 0x83, 0xD9, 0xEC, 0x25, 0x6C, 0x48, 0x48, 0x72};
static int sdHashKey4[] = {0xD8, 0xC0, 0xB0, 0xF3, 0x3E, 0x6B, 0x76, 0x85, 0xFD, 0xFB, 0x4D, 0x7D, 0x45, 0x1E, 0x92, 0x03};
static int sdHashKey5[] = {0xCB, 0x15, 0xF4, 0x07, 0xF9, 0x6A, 0x52, 0x3C, 0x04, 0xB9, 0xB2, 0xEE, 0x5C, 0x53, 0xFA, 0x86};
static int sdHashKey6[] = {0x70, 0x44, 0xA3, 0xAE, 0xEF, 0x5D, 0xA5, 0xF2, 0x85, 0x7F, 0xF2, 0xD6, 0x94, 0xF5, 0x36, 0x3B};
static int sdHashKey7[] = {0xEC, 0x6D, 0x29, 0x59, 0x26, 0x35, 0xA5, 0x7F, 0x97, 0x2A, 0x0D, 0xBC, 0xA3, 0x26, 0x33, 0x00};

typedef struct{
		int mode;
		uint8_t key[16];
		uint8_t pad[16];
		unsigned int padSize;
} _SD_Ctx1, *SD_Ctx1;

typedef struct{
		int mode;
		int unk;
		uint8_t buf[16];
} _SD_Ctx2, *SD_Ctx2;

static int isNullKey(uint8_t* key) {
	if (key != NULL) {
		for (int i=0; i < 0x10; i++) {
			if (key[i] != (uint8_t) 0) {
				return 0;
			}
		}
	}
	return 1;
}

static void xorHash(uint8_t* dest, int dest_offset, int* src, int src_offset, int size) {
	for (int i=0; i < size; i++) {
		dest[dest_offset + i] = (uint8_t) (dest[dest_offset + i] ^ src[src_offset + i]);
	}
}

static void xorKey(uint8_t* dest, int dest_offset, uint8_t* src, int src_offset, int size) {
	for (int i=0; i < size; i++) {
		dest[dest_offset + i] = (uint8_t) (dest[dest_offset + i] ^ src[src_offset + i]);
	}
}

static void ScrambleSD(uint8_t *buf, int size, int seed, int cbc, int kirk_code) {
	KIRK_AES128CBC_HEADER *header = (KIRK_AES128CBC_HEADER*)buf;

	// Set CBC mode.
	header->mode = cbc;

	// Set unkown parameters to 0.
	header->unk_4 = 0;
	header->unk_8 = 0;

	// Set the the key seed to seed.
	header->keyseed = seed;

	// Set the the data size to size.
	header->data_size = size;

	sceUtilsBufferCopyWithRange(buf, size, buf, size, kirk_code);
	if(kirk_code==KIRK_CMD_ENCRYPT_IV_0)
		memmove(buf,buf+20,size);
}

static int getModeSeed(int mode) {
	int seed;
	switch (mode) {
		case 0x6:
			seed = 0x11;
			break;
		case 0x4:
			seed = 0xD;
			break;
		case 0x2:
			seed = 0x5;
			break;
		case 0x1:
			seed = 0x3;
			break;
		case 0x3:
			seed = 0xC;
			break;
		default:
			seed = 0x10;
			break;
	}
	return seed;
}

static void cryptMember(SD_Ctx2 ctx, uint8_t* data, int data_offset, int length) {
	int finalSeed;
	uint8_t *dataBuf = malloc(length + 0x14);
	uint8_t keyBuf[0x10 + 0x10];
	uint8_t hashBuf[0x10];

	memset(dataBuf,0,length + 0x14);
	memset(keyBuf,0,sizeof(keyBuf));
	memset(hashBuf,0,sizeof(hashBuf));

	// Copy the hash stored by hleSdCreateList.
	arraycopy(ctx->buf, 0, dataBuf, 0x14, 0x10);

	if (ctx->mode == 0x1) {
		// Decryption mode 0x01: decrypt the hash directly with KIRK CMD7.
		ScrambleSD(dataBuf, 0x10, 0x4, KIRK_MODE_DECRYPT_CBC, KIRK_CMD_DECRYPT_IV_0);
		finalSeed = 0x53;
	} else if (ctx->mode == 0x5) {
		// Decryption mode 0x05: XOR the hash with new SD keys and decrypt with KIRK CMD7.
		xorHash(dataBuf, 0x14, sdHashKey7, 0, 0x10);
		ScrambleSD(dataBuf, 0x10, 0x12, KIRK_MODE_DECRYPT_CBC, KIRK_CMD_DECRYPT_IV_0);
		xorHash(dataBuf, 0, sdHashKey6, 0, 0x10);
		finalSeed = 0x64;
	} else {
		// unsupported mode
	}

	// Store the calculated key.
	arraycopy(dataBuf, 0, keyBuf, 0x10, 0x10);

	// Apply extra padding if ctx.unk is not 1.
	if (ctx->unk != 0x1) {
		arraycopy(keyBuf, 0x10, keyBuf, 0, 0xC);
		keyBuf[0xC] = (uint8_t) ((ctx->unk - 1) & 0xFF);
		keyBuf[0xD] = (uint8_t) (((ctx->unk - 1) >> 8) & 0xFF);
		keyBuf[0xE] = (uint8_t) (((ctx->unk - 1) >> 16) & 0xFF);
		keyBuf[0xF] = (uint8_t) (((ctx->unk - 1) >> 24) & 0xFF);
	}

	// Copy the first 0xC bytes of the obtained key and replicate them
	// across a new list buffer. As a terminator, add the ctx1.seed parameter's
	// 4 bytes (endian swapped) to achieve a full numbered list.
	for (int i=0x14; i < (length + 0x14); i += 0x10) {
		arraycopy(keyBuf, 0x10, dataBuf, i, 0xC);
		dataBuf[i + 0xC] = (uint8_t) (ctx->unk & 0xFF);
		dataBuf[i + 0xD] = (uint8_t) ((ctx->unk >> 8) & 0xFF);
		dataBuf[i + 0xE] = (uint8_t) ((ctx->unk >> 16) & 0xFF);
		dataBuf[i + 0xF] = (uint8_t) ((ctx->unk >> 24) & 0xFF);
		ctx->unk++;
	}

	arraycopy(dataBuf, length + 0x04, hashBuf, 0, 0x10);

	ScrambleSD(dataBuf, length, finalSeed, KIRK_MODE_DECRYPT_CBC, KIRK_CMD_DECRYPT_IV_0);

	// XOR the first 16-bytes of data with the saved key to generate a new hash.
	xorKey(dataBuf, 0, keyBuf, 0, 0x10);

	// Copy back the last hash from the list to the first half of keyBuf.
	arraycopy(hashBuf, 0, keyBuf, 0, 0x10);

	// Finally, XOR the full list with the given data.
	xorKey(data, data_offset, dataBuf, 0, length);
	free(dataBuf);
}

/*
 * sceSd - chnnlsv.prx
 */
static int hleSdSetIndex(SD_Ctx1 ctx, int encMode) {
	// Set all parameters to 0 and assign the encMode.
	ctx->mode = encMode;
	return 0;
}

static int hleSdCreateList(SD_Ctx2 ctx, int encMode, int genMode, uint8_t* data, uint8_t* key) {
	// If the key is not a 16-uint8_t key, return an error.
	//if (key.length < 0x10) {
	//	return -1;
	//}

	// Set the mode and the unknown parameters.
	ctx->mode = encMode;
	ctx->unk = 0x1;

	// Key generator mode 0x1 (encryption): use an encrypted pseudo random number before XORing the data with the given key.
	if (genMode == 0x1) {
		uint8_t header[0x25];
		uint8_t seed[0x14];

		// Generate SHA-1 to act as seed for encryption.
		//ByteBuffer bSeed = ByteBuffer.wrap(seed);
		sceUtilsBufferCopyWithRange(seed, 0x14, NULL, 0, KIRK_CMD_PRNG);
		
		// Propagate SHA-1 in kirk header.
		arraycopy(seed, 0, header, 0, 0x10);
		arraycopy(seed, 0, header, 0x14, 0x10);

		// Encryption mode 0x1: encrypt with KIRK CMD4 and XOR with the given key.
		if (ctx->mode == 0x1) {
			ScrambleSD(header, 0x10, 0x4, KIRK_MODE_ENCRYPT_CBC, KIRK_CMD_ENCRYPT_IV_0);
			arraycopy(header, 0, ctx->buf, 0, 0x10);
			arraycopy(header, 0, data, 0, 0x10);
			// If the key is not null, XOR the hash with it.
			if (!isNullKey(key)) {
				xorKey(ctx->buf, 0, key, 0, 0x10);
			}
			return 0;
		} else if (ctx->mode == 0x5) { // Encryption mode 0x5: XOR with new SD keys, encrypt with KIRK CMD4 and XOR with the given key.
			xorHash(header, 0x14, sdHashKey6, 0, 0x10);
			ScrambleSD(header, 0x10, 0x12, KIRK_MODE_ENCRYPT_CBC, KIRK_CMD_ENCRYPT_IV_0);
			xorHash(header, 0, sdHashKey7, 0, 0x10);
			arraycopy(header, 0, ctx->buf, 0, 0x10);
			arraycopy(header, 0, data, 0, 0x10);
			// If the key is not null, XOR the hash with it.
			if (!isNullKey(key)) {
				xorKey(ctx->buf, 0, key, 0, 0x10);
			}
			return 0;
		} else {
			// unsupported mode
			return (-1);
		}
	} else if (genMode == 0x2) { // Key generator mode 0x02 (decryption): directly XOR the data with the given key.
		// Grab the data hash (first 16-bytes).
		arraycopy(data, 0, ctx->buf, 0, 0x10);
		// If the key is not null, XOR the hash with it.
		if (!isNullKey(key)) {
			xorKey(ctx->buf, 0, key, 0, 0x10);
		}
		return 0;
	} else {
		// Invalid mode.
		return -1;
	}
}

static int hleSdRemoveValue(SD_Ctx1 ctx, uint8_t *data, int length) {
	if (ctx->padSize > 0x10 || (length < 0)) {
		// Invalid key or length.
		return -1;
	} else if (((ctx->padSize + length) <= 0x10)) {
		// The key hasn't been set yet.
		// Extract the hash from the data and set it as the key.
		arraycopy(data, 0, ctx->pad, ctx->padSize, length);
		ctx->padSize += length;
		return 0;
	} else {
		// Calculate the seed.
		int seed = getModeSeed(ctx->mode);

		// Setup the buffers.
		uint8_t *scrambleBuf = malloc((length + ctx->padSize) + 0x14);

		// Copy the previous key to the buffer.
		arraycopy(ctx->pad, 0, scrambleBuf, 0x14, ctx->padSize);

		// Calculate new key length.
		int kLen = ctx->padSize;

		ctx->padSize += length;
		ctx->padSize &= 0x0F;
		if (ctx->padSize == 0) {
			ctx->padSize = 0x10;
		}

		// Calculate new data length.
		length -= ctx->padSize;

		// Copy data's footer to make a new key.
		arraycopy(data, length, ctx->pad, 0, ctx->padSize);

		// Process the encryption in 0x800 blocks.
		int blockSize = 0;
		int dataOffset = 0;

		while (length > 0) {
			blockSize = (length + kLen >= 0x800) ? 0x800 : length + kLen;

			arraycopy(data, dataOffset, scrambleBuf, 0x14 + kLen, blockSize - kLen);

			// Encrypt with KIRK CMD 4 and XOR with result.
			xorKey(scrambleBuf, 0x14, ctx->key, 0, 0x10);
			ScrambleSD(scrambleBuf, blockSize, seed, KIRK_MODE_ENCRYPT_CBC, KIRK_CMD_ENCRYPT_IV_0);
			arraycopy(scrambleBuf, (blockSize + 0x4) - 0x14, ctx->key, 0, 0x10);

			// Adjust data length, data offset and reset any key length.
			length -= (blockSize - kLen);
			dataOffset += (blockSize - kLen);
			kLen = 0;
		}
		free(scrambleBuf);
		return 0;
	}
}

static int hleSdGetLastIndex(SD_Ctx1 ctx, uint8_t *hash, uint8_t *key) {
	int i;
	if (ctx->padSize > 0x10) {
		// Invalid key length.
		return -1;
	}

	// Setup the buffers.
	uint8_t scrambleEmptyBuf[0x10 + 0x14];
	uint8_t keyBuf[0x10];
	uint8_t scrambleKeyBuf[0x10 + 0x14];
	uint8_t resultBuf[0x10];
	uint8_t scrambleResultBuf[0x10 + 0x14];
	uint8_t scrambleResultKeyBuf[0x10 + 0x14];

	memset(scrambleEmptyBuf,0,sizeof(scrambleEmptyBuf));
	memset(keyBuf,0,sizeof(keyBuf));
	memset(scrambleKeyBuf,0,sizeof(scrambleKeyBuf));
	memset(resultBuf,0,sizeof(resultBuf));
	memset(scrambleResultBuf,0,sizeof(scrambleResultBuf));
	memset(scrambleResultKeyBuf,0,sizeof(scrambleResultKeyBuf));

	// Calculate the seed.
	int seed = getModeSeed(ctx->mode);

	// Encrypt an empty buffer with KIRK CMD 4.
	ScrambleSD(scrambleEmptyBuf, 0x10, seed, KIRK_MODE_ENCRYPT_CBC, KIRK_CMD_ENCRYPT_IV_0);
	arraycopy(scrambleEmptyBuf, 0, keyBuf, 0, 0x10);

	// Apply custom padding management.
	uint8_t b = ((keyBuf[0] & (uint8_t) 0x80) != 0) ? (uint8_t) 0x87 : 0;
	for (i = 0; i < 0xF; i++) {
		keyBuf[i] = (uint8_t) ((keyBuf[i] << 1) | ((keyBuf[i + 1] >> 7) & 0x01));
	}
	keyBuf[0xF] = (uint8_t) ((keyBuf[0xF] << 1) ^ b);

	if (ctx->padSize < 0x10) {
		uint8_t bb = ((keyBuf[0] & (uint8_t) 0x80) != 0) ? (uint8_t) 0x87 : 0;
		for (i = 0; i < 0xF; i++) {
			keyBuf[i] = (uint8_t) ((keyBuf[i] << 1) | ((keyBuf[i + 1] >> 7) & 0x01));
		}
		keyBuf[0xF] = (uint8_t) ((keyBuf[0xF] << 1) ^ bb);

		ctx->pad[ctx->padSize] = (uint8_t) 0x80;
		if ((ctx->padSize + 1) < 0x10) {
			for (i = 0; i < (0x10 - ctx->padSize - 1); i++) {
				ctx->pad[ctx->padSize + 1 + i] = 0;
			}
		}
	}

	// XOR previous key with new one.
	xorKey(ctx->pad, 0, keyBuf, 0, 0x10);

	arraycopy(ctx->pad, 0, scrambleKeyBuf, 0x14, 0x10);
	arraycopy(ctx->key, 0, resultBuf, 0, 0x10);

	// Encrypt with KIRK CMD 4 and XOR with result.
	xorKey(scrambleKeyBuf, 0x14, resultBuf, 0, 0x10);
	ScrambleSD(scrambleKeyBuf, 0x10, seed, KIRK_MODE_ENCRYPT_CBC, KIRK_CMD_ENCRYPT_IV_0);
	arraycopy(scrambleKeyBuf, (0x10 + 0x4) - 0x14, resultBuf, 0, 0x10);

	// If ctx.mode is new mode 0x5 or 0x6, XOR with the new hash key 5, else, XOR with hash key 2.
	if ((ctx->mode == 0x5) || (ctx->mode == 0x6)) {
		xorHash(resultBuf, 0, sdHashKey5, 0, 0x10);
	} else if ((ctx->mode == 0x3) || (ctx->mode == 0x4)) {
		xorHash(resultBuf, 0, sdHashKey2, 0, 0x10);
	}

	// If mode is 2, 4 or 6, encrypt again with KIRK CMD 5 and then KIRK CMD 4.
	if ((ctx->mode == 0x2) || (ctx->mode == 0x4) || (ctx->mode == 0x6)) {
		arraycopy(resultBuf, 0, scrambleResultBuf, 0x14, 0x10);
		ScrambleSD(scrambleResultBuf, 0x10, 0x100, KIRK_MODE_ENCRYPT_CBC, KIRK_CMD_ENCRYPT_IV_FUSE);
		arraycopy(scrambleResultBuf, 0, scrambleResultBuf, 0x14, 0x10);
		for(i=0; i < 0x14; i++) {
			scrambleResultBuf[i] = 0;
		}
		ScrambleSD(scrambleResultBuf, 0x10, seed, KIRK_MODE_ENCRYPT_CBC, KIRK_CMD_ENCRYPT_IV_0);
		arraycopy(scrambleResultBuf, 0, resultBuf, 0, 0x10);
	}

	// XOR with the supplied key and encrypt with KIRK CMD 4.
	if (key != NULL) {
		xorKey(resultBuf, 0, key, 0, 0x10);
		arraycopy(resultBuf, 0, scrambleResultKeyBuf, 0x14, 0x10);
		ScrambleSD(scrambleResultKeyBuf, 0x10, seed, KIRK_MODE_ENCRYPT_CBC, KIRK_CMD_ENCRYPT_IV_0);
		arraycopy(scrambleResultKeyBuf, 0, resultBuf, 0, 0x10);
	}

	// Copy back the generated hash.
	arraycopy(resultBuf, 0, hash, 0, 0x10);

	// Clear the context fields.
	memset(ctx,0,sizeof(_SD_Ctx1));

	return 0;
}

static int hleSdSetMember(SD_Ctx2 ctx, uint8_t* data, int length) {
	if (length <= 0) {
		return -1;
	}

	// Parse the data in 0x800 blocks first.
	int index = 0;
	if (length >= 0x800) {
		for (index = 0; length >= 0x800; index += 0x800) {
			cryptMember(ctx, data, index, 0x800);
			length -= 0x800;
		}
	}

	// Finally parse the rest of the data.
	if (length)
		cryptMember(ctx, data, index, length);

	return 0;
}

static void DecryptSavedata(uint8_t *buf, int size, uint8_t *key) {
	// Initialize the context structs.
	int sdDecMode;
	_SD_Ctx1 ctx1;
	_SD_Ctx2 ctx2;
	memset(&ctx1,0,sizeof(ctx1));
	memset(&ctx2,0,sizeof(ctx2));

	// Setup the buffers.
	int alignedSize = ((size + 0xF) >> 4) << 4;
	uint8_t *tmpbuf = malloc(alignedSize);
	//uint8_t hash[0x10];

	// Set the decryption mode.
	if (isNullKey(key)) {
		sdDecMode = 1;
	} else {
		// After firmware version 2.5.2 the decryption mode used is 5.
		//if (Emulator.getInstance().getFirmwareVersion() > 252) {
		sdDecMode = 5;
		//} else {
		//	sdDecMode = 3;
		//}
	}

	// Perform the decryption.
	hleSdSetIndex(&ctx1, sdDecMode);
	hleSdCreateList(&ctx2, sdDecMode, 2, buf, key);
	hleSdRemoveValue(&ctx1, buf, 0x10);

	arraycopy(buf, 0x10, tmpbuf, 0, size - 0x10);
	hleSdRemoveValue(&ctx1, tmpbuf, alignedSize);

	hleSdSetMember(&ctx2, tmpbuf, alignedSize);

	// Clear context 2.
	memset(&ctx2,0,sizeof(_SD_Ctx2));

	// Generate a file hash for this data.
	//hleSdGetLastIndex(&ctx1, hash, key);

	// Copy back the data.
	arraycopy(tmpbuf, 0, buf, 0, size - 0x10);
	free(tmpbuf);
}

static void EncryptSavedata(uint8_t* buf, int size, uint8_t *key, uint8_t *hash, uint8_t *iv) {
	// Initialize the context structs.
	int sdEncMode;
	_SD_Ctx1 ctx1;
	_SD_Ctx2 ctx2;
	memset(&ctx1,0,sizeof(ctx1));
	memset(&ctx2,0,sizeof(ctx2));

	// Setup the buffers.
	int alignedSize = ((size + 0xF) >> 4) << 4;
	uint8_t header[0x10];
	uint8_t *tmpbuf = malloc(alignedSize);

	memset(header,0,sizeof(header));
	memset(tmpbuf,0,alignedSize);

	// Copy the plain data to tmpbuf.
	arraycopy(buf, 0, tmpbuf, 0, size);

	// Set the encryption mode.
	if (isNullKey(key)) {
		sdEncMode = 1;
	} else {
		// After firmware version 2.5.2 the encryption mode used is 5.
		//if (Emulator.getInstance().getFirmwareVersion() > 252) {
		sdEncMode = 5;
		//} else {
			//sdEncMode = 3;
		//}
	}

	// Generate the encryption IV (first 0x10 bytes).
	if(!iv){
		hleSdCreateList(&ctx2, sdEncMode, 1, header, key);
	}else{
		ctx2.mode = sdEncMode;
		ctx2.unk = 0x1;
		memcpy(ctx2.buf,iv,0x10);
		if (!isNullKey(key)) {
			xorKey(ctx2.buf, 0, key, 0, 0x10);
		}
		memcpy(header,iv,0x10); //actually the same
	}
	hleSdSetIndex(&ctx1, sdEncMode);
	hleSdRemoveValue(&ctx1, header, 0x10);

	hleSdSetMember(&ctx2, tmpbuf, alignedSize);

	// Clear extra bytes.
	for (int i = size; i < alignedSize; i++) {
		tmpbuf[i] = 0;
	}

	// Encrypt the data.
	hleSdRemoveValue(&ctx1, tmpbuf, alignedSize);

	// Copy back the encrypted data + IV.
	arraycopy(header, 0, buf, 0, 0x10);
	arraycopy(tmpbuf, 0, buf, 0x10, size);

	// Clear context 2.
	memset(&ctx2,0,sizeof(_SD_Ctx2));

	// Generate a file hash for this data.
	hleSdGetLastIndex(&ctx1, hash, key);
	free(tmpbuf);
}

static void GenerateSavedataHash(uint8_t *data, int size, int mode, uint8_t* key, uint8_t *hash) {
	_SD_Ctx1 ctx1;
	memset(&ctx1,0,sizeof(ctx1));

	// Generate a new hash using a key.
	hleSdSetIndex(&ctx1, mode);
	hleSdRemoveValue(&ctx1, data, size);
	if(hleSdGetLastIndex(&ctx1, hash, NULL)<0)
		memset(hash,1,0x10);

	//return hash;
}

static void UpdateSavedataHashes(uint8_t* savedataParams, uint8_t* data, int size) {
	// Setup the params, hashes, modes and key (empty).
	uint8_t key[0x10];
	memset(key,0,sizeof(key));

	int mode = 2;
	int check_bit = 1;

	// Check for previous SAVEDATA_PARAMS data in the file.
	//Object savedataParamsOld = psf.get("SAVEDATA_PARAMS");
	//if (savedataParamsOld != null) {
		// Extract the mode setup from the already existing data.
		//byte[] savedataParamsOldArray = (byte[]) savedataParamsOld;
	mode = ((savedataParams[0] >> 4) & 0xF);
	check_bit = ((savedataParams[0]) & 0xF);
	//}
	memset(savedataParams,0,0x80);
	//if((mode&0x4)==0x4)mode=2;

	if ((mode & 0x4) == 0x4) {
		// Generate a type 6 hash.
		GenerateSavedataHash(data, size, 6, key, savedataParams+0x20);
		savedataParams[0]|=0x01;

		savedataParams[0]|=0x40;
		// Generate a type 5 hash.
		GenerateSavedataHash(data, size, 5, key, savedataParams+0x70);
	} else if((mode & 0x2) == 0x2) {
		// Generate a type 4 hash.
		GenerateSavedataHash(data, size, 4, key, savedataParams+0x20);
		savedataParams[0]|=0x01;

		savedataParams[0]|=0x20;
		// Generate a type 3 hash.
		GenerateSavedataHash(data, size, 3, key, savedataParams+0x70);
	} else {
		// Generate a type 2 hash.
		GenerateSavedataHash(data, size, 2, key, savedataParams+0x20);
		savedataParams[0]|=0x01;
	}

	if ((check_bit & 0x1) == 0x1) {
		// Generate a type 1 hash.
		GenerateSavedataHash(data, size, 1, key, savedataParams+0x10);
	}
}

int read_psp_game_key(const char* fkey, uint8_t* key)
{
	uint8_t* inbuf;
	size_t size;

	LOG("Loading key %s", fkey);
	if (read_buffer(fkey, &inbuf, &size) != 0)
		return 0;

	switch (size)
	{
	case 0x10:
		// SGKeyDumper
		memcpy(key, inbuf, 0x10);
		break;

	case 0x600:
		// SGDeemer
		memcpy(key, inbuf + 0x5DC, 0x10);
		break;

	default:
		memset(key, 0, 0x10);
		break;
	}

	free(inbuf);
	return 1;
}

static uint8_t* find_sfo_parameter(uint8_t* p, const char* name)
{
	int label_offset=read32(p+8);
	int data_offset=read32(p+12);
	int nlabel=read32(p+16);

	for(int i=0; i<nlabel; i++)
		if(strcmp((char*)p+label_offset+read16(p+20+16*i), name) == 0)
			return (p+data_offset+read32(p+20+16*i+12));

	return NULL;
}

/* Find the named file inside the FILE_LIST, and return
   an absolute pointer to it. */
static uint8_t* find_sfo_datafile(uint8_t *filelist, const char *name)
{
    /* Process all files */
    for (int i = 0; (i + 0x0d) <= 0xC60; i += 0x20)
        /* Check if this is the filename we want */
        if (strcmp((char *)filelist + i, name) == 0)
            return (filelist+i);

    /* File was not found if it makes it here */
    return NULL;
}

int psp_EncryptSavedata(const char* fpath, const char* fname, uint8_t* key)
{
	uint8_t* sfo;
	uint8_t* inbuf;
	char path[256];
	size_t size, sfosize;

	kirk_init();

	snprintf(path, sizeof(path), "%s%s", fpath, fname);
	LOG("Loading file %s", path);
	FILE *f = fopen(path, "rb");
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	inbuf = calloc(1, size + 0x10);
	fread(inbuf, 1, size, f);
	fclose(f);

	snprintf(path, sizeof(path), "%sPARAM.SFO", fpath);
	LOG("Loading file %s", path);
	if (read_buffer(path, &sfo, &sfosize) != 0)
	{
		free(inbuf);
		return 0;
	}

	if(read32(sfo) != 0x46535000 || read32(sfo+4) != 0x00000101)
	{
		free(inbuf);
		free(sfo);
		return 0;
	}

	uint8_t* sd_param = find_sfo_parameter(sfo, "SAVEDATA_PARAMS");
	uint8_t* sd_flist = find_sfo_parameter(sfo, "SAVEDATA_FILE_LIST");
	sd_flist = find_sfo_datafile(sd_flist, fname);

	EncryptSavedata(inbuf, size, key, sd_flist + 0x0d, (uint8_t*)"bucanero.com.ar");

	//This hash is different from original one, but PSP somehow accepts it...
	UpdateSavedataHashes(sd_param, sfo, sfosize);

	LOG("Saving updated file %s", path);
	if (write_buffer(path, sfo, sfosize) != 0)
		return 0;

	snprintf(path, sizeof(path), "%s%s", fpath, fname);
	LOG("Saving encrypted file %s", path);
	if (write_buffer(path, inbuf, size+0x10) != 0)
		return 0;

	free(inbuf);
	free(sfo);
	return 1;
}

int psp_DecryptSavedata(const char* fname, uint8_t* key)
{
	uint8_t* inbuf;
	size_t size;

	kirk_init();

	LOG("Loading file %s", fname);
	if (read_buffer(fname, &inbuf, &size) != 0)
		return 0;

	DecryptSavedata(inbuf, size, key);

	LOG("Saving decrypted file %s", fname);
	if (write_buffer(fname, inbuf, size-0x10) != 0)
		return 0;

	free(inbuf);
	return 1;
}

/*
	"[Proof of Concept/alpha] PSP Savedata En/Decrypter on PC (GPLv3+)\n"
	"kirk-engine (C) draan / proxima\n"
	"jpcsp (C) jpcsp team, especially CryptoEngine by hykem\n"
	"ported by popsdeco (aka @cielavenir)\n"
	"acknowledgement: referred SED-PC to fix the hashing algorithm\n"
	"\n"
	"Decrypt: endecrypter ENC.bin GAMEKEY.bin > DEC.bin\n"
	"Encrypt: endecrypter DEC.bin GAMEKEY.bin PARAM.SFO > ENC.bin\n"
	"Please note that PARAM.SFO is overwritten in encryption.\n"
*/
