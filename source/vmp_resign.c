/*
*  vita-mcr2vmp by @dots_tb - https://github.com/dots-tb/vita-mcr2vmp/
*  signs PSOne MCR files to create VMP files for use with Sony Vita/PSP and exports MCR files from VMP
*  With help from the CBPS (https://discord.gg/2nDCbxJ) , especially:
*  @AnalogMan151, @teakhanirons
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <polarssl/aes.h>

#include "utils.h"
#include "saves.h"
#include "shiftjis.h"

#define PSV_TYPE_PS1    0x01
#define PSV_TYPE_PS2    0x02
#define PSV_SEED_OFFSET 0x08
#define PSV_HASH_OFFSET 0x1C
#define PSV_TYPE_OFFSET 0x3C
#define VMP_SEED_OFFSET 0x0C
#define VMP_HASH_OFFSET 0x20
#define MCR_OFFSET      0x80
#define MCR_MAGIC       0x0000434D
#define VMP_MAGIC       0x564D5000
#define PSV_MAGIC       0x50535600
#define VMP_SIZE        0x20080
#define MC_SIZE         0x20000

static const char SJIS_REPLACEMENT_TABLE[] = 
    " ,.,..:;?!\"*'`*^"
    "-_????????*---/\\"
    "~||--''\"\"()()[]{"
    "}<><>[][][]+-+X?"
    "-==<><>????*'\"CY"
    "$c&%#&*@S*******"
    "*******T><^_'='";

static const uint8_t vmp_ps1key[0x10] = {
	0xAB, 0x5A, 0xBC, 0x9F, 0xC1, 0xF4, 0x9D, 0xE6, 0xA0, 0x51, 0xDB, 0xAE, 0xFA, 0x51, 0x88, 0x59
};

static const uint8_t vmp_iv[0x10] = {
	0xB3, 0x0F, 0xFE, 0xED, 0xB7, 0xDC, 0x5E, 0xB7, 0x13, 0x3D, 0xA6, 0x0D, 0x1B, 0x6B, 0x2C, 0xDC
};


static void XorWithIv(uint8_t* buf, const uint8_t* Iv)
{
	// The block in AES is always 128bit no matter the key size
	for (uint8_t i = 0; i < 16; ++i)
		buf[i] ^= Iv[i];
}
 
//Derive the 0x40-byte HMAC key from the seed, then sign `input` in place.
//`dest` points at the signature field inside `input`, which is zeroed before
//hashing -- so the same routine both produces and checks a signature.
static void generateHash(const uint8_t *input, const uint8_t *salt_seed, uint8_t *dest, size_t sz)
{
	aes_context aes_ctx;
	uint8_t salt[0x40];
	uint8_t work_buf[0x14];

	memset(salt , 0, sizeof(salt));
	memset(&aes_ctx, 0, sizeof(aes_context));

	//idk why the normal cbc doesn't work.
	memcpy(work_buf, salt_seed, 0x10);

	aes_setkey_dec(&aes_ctx, vmp_ps1key, 128);
	aes_crypt_ecb(&aes_ctx, AES_DECRYPT, work_buf, salt);
	aes_setkey_enc(&aes_ctx, vmp_ps1key, 128);
	aes_crypt_ecb(&aes_ctx, AES_ENCRYPT, work_buf, salt + 0x10);

	XorWithIv(salt, vmp_iv);

	memset(work_buf, 0xFF, sizeof(work_buf));
	memcpy(work_buf, salt_seed + 0x10, 0x4);

	XorWithIv(salt + 0x10, work_buf);
	
	memset(salt + 0x14, 0, sizeof(salt) - 0x14);

	//The signature is HMAC-SHA1 over the whole file, keyed by the salt, with
	//the signature field itself zeroed. The salt is exactly one SHA-1 block,
	//so the key is used as it stands and no normalisation happens.
	memset(dest, 0, 0x14);
	calculate_hmac_hash(input, sz, salt, sizeof(salt), dest);
}

int vmp_resign(const char *src_vmp)
{
	size_t sz;
	uint8_t *input;

	LOG("=====Vita MCR2VMP by @dots_tb=====");

	if (read_buffer(src_vmp, &input, &sz) < 0) {
		LOG("Failed to open input file");
		return 0;
	}

	if (sz != VMP_SIZE || *(uint32_t*)input != VMP_MAGIC) {
		LOG("Not a VMP file");
		free(input);
		return 0;
	}

	//Stamping the seed is part of signing, not of hashing: the signature
	//covers the file as it will be written, seed included.
	memcpy(input + VMP_SEED_OFFSET, "www.bucanero.com.ar", 20);

	LOG("Signing VMP Memory Card File...");
	generateHash(input, input + VMP_SEED_OFFSET, input + VMP_HASH_OFFSET, sz);

	LOG("New signature:");
	dump_data(input+VMP_HASH_OFFSET, 20);

	if (write_buffer(src_vmp, input, sz) < 0) {
		LOG("Failed to open output file");
		free(input);
		return 0;
	}

	free(input);
	LOG("VMP resigned successfully: %s", src_vmp);

	return 1;
}

int psv_verify(uint8_t *psv, size_t len)
{
	uint8_t stored[0x14], computed[0x14];
	int signed_at_all = 0;

	if (len < 0x84 || memcmp(psv, "\0VSP", 4) != 0)
		return PSV_SIG_UNKNOWN;

	//Only the PS1 derivation is implemented here, which is the only type
	//psv_resign() writes. A PS2 .PSV is a save we cannot check, not a bad one.
	if (psv[PSV_TYPE_OFFSET] != PSV_TYPE_PS1)
		return PSV_SIG_UNKNOWN;

	memcpy(stored, psv + PSV_HASH_OFFSET, sizeof(stored));

	for (int i = 0; i < (int)sizeof(stored); i++)
		if (stored[i]) {
			signed_at_all = 1;
			break;
		}

	if (!signed_at_all)
		return PSV_SIG_UNSIGNED;

	//generateHash() writes into the file's own signature field, which is
	//also what it has to hash as zeros. Let it, then put the original back
	//so the caller's buffer comes out exactly as it went in.
	generateHash(psv, psv + PSV_SEED_OFFSET, psv + PSV_HASH_OFFSET, len);
	memcpy(computed, psv + PSV_HASH_OFFSET, sizeof(computed));
	memcpy(psv + PSV_HASH_OFFSET, stored, sizeof(stored));

	return memcmp(stored, computed, sizeof(stored)) == 0 ? PSV_SIG_OK : PSV_SIG_BAD;
}

int psv_resign(const char *src_psv)
{
	size_t sz;
	uint8_t *input;

	if (read_buffer(src_psv, &input, &sz) < 0) {
		LOG("Failed to open input file");
		return 0;
	}

	if (sz < 0x2000 || *(uint32_t*)input != PSV_MAGIC || input[PSV_TYPE_OFFSET] != 0x01) {
		LOG("Not a PS1 PSV file");
		free(input);
		return 0;
	}

	memcpy(input + PSV_SEED_OFFSET, "www.bucanero.com.ar", 20);

	LOG("Signing PSV Save File...");
	generateHash(input, input + PSV_SEED_OFFSET, input + PSV_HASH_OFFSET, sz);

	LOG("New signature:");
	dump_data(input + PSV_HASH_OFFSET, 20);

	if (write_buffer(src_psv, input, sz) < 0) {
		LOG("Failed to open output file");
		free(input);
		return 0;
	}

	free(input);
	LOG("PSV resigned successfully: %s", src_psv);

	return 1;
}

//Convert Shift-JIS characters to ASCII equivalent
static void sjis2ascii(char* bData)
{
	uint16_t ch;
	int i, j = 0;
	int len = strlen(bData);
	
	for (i = 0; i < len; i += 2)
	{
		ch = (bData[i]<<8) | bData[i+1];

		// 'A' .. 'Z'
		// '0' .. '9'
		if ((ch >= 0x8260 && ch <= 0x8279) || (ch >= 0x824F && ch <= 0x8258))
		{
			bData[j++] = (ch & 0xFF) - 0x1F;
			continue;
		}

		// 'a' .. 'z'
		if (ch >= 0x8281 && ch <= 0x829A)
		{
			bData[j++] = (ch & 0xFF) - 0x20;
			continue;
		}

		if (ch >= 0x8140 && ch <= 0x81AC)
		{
			bData[j++] = SJIS_REPLACEMENT_TABLE[(ch & 0xFF) - 0x40];
			continue;
		}

		if (ch == 0x0000)
		{
			//End of the string
			bData[j] = 0;
			return;
		}

		// Character not found
		bData[j++] = bData[i];
		bData[j++] = bData[i+1];
	}

	bData[j] = 0;
	return;
}

// PSV files (PS1/PS2) savegame titles are stored in Shift-JIS
char* sjis2utf8(char* input)
{
    // Simplify the input and decode standard ASCII characters
    sjis2ascii(input);

    int len = strlen(input);
    char* output = malloc(3 * len); //ShiftJis won't give 4byte UTF8, so max. 3 byte per input char are needed
    size_t indexInput = 0, indexOutput = 0;

    while(indexInput < len)
    {
        char arraySection = ((uint8_t)input[indexInput]) >> 4;

        size_t arrayOffset;
        if(arraySection == 0x8) arrayOffset = 0x100; //these are two-byte shiftjis
        else if(arraySection == 0x9) arrayOffset = 0x1100;
        else if(arraySection == 0xE) arrayOffset = 0x2100;
        else arrayOffset = 0; //this is one byte shiftjis

        //determining real array offset
        if(arrayOffset)
        {
            arrayOffset += (((uint8_t)input[indexInput]) & 0xf) << 8;
            indexInput++;
            if(indexInput >= len) break;
        }
        arrayOffset += (uint8_t)input[indexInput++];
        arrayOffset <<= 1;

        //unicode number is...
        uint16_t unicodeValue = (shiftJIS_convTable[arrayOffset] << 8) | shiftJIS_convTable[arrayOffset + 1];

        //converting to UTF8
        if(unicodeValue < 0x80)
        {
            output[indexOutput++] = unicodeValue;
        }
        else if(unicodeValue < 0x800)
        {
            output[indexOutput++] = 0xC0 | (unicodeValue >> 6);
            output[indexOutput++] = 0x80 | (unicodeValue & 0x3f);
        }
        else
        {
            output[indexOutput++] = 0xE0 | (unicodeValue >> 12);
            output[indexOutput++] = 0x80 | ((unicodeValue & 0xfff) >> 6);
            output[indexOutput++] = 0x80 | (unicodeValue & 0x3f);
        }
    }

	//remove the unnecessary bytes
    output[indexOutput] = 0;
    return output;
}
