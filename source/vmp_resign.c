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
#include <polarssl/sha1.h>

#include "utils.h"

#define VMP_SEED_OFFSET 0xC
#define VMP_HASH_OFFSET 0x20
#define MCR_OFFSET      0x80
#define MCR_MAGIC       0x0000434D
#define VMP_MAGIC       0x564D5000
#define VMP_SIZE        0x20080
#define MC_SIZE         0x20000

static const uint8_t vmp_ps1key[0x10] = {
	0xAB, 0x5A, 0xBC, 0x9F, 0xC1, 0xF4, 0x9D, 0xE6, 0xA0, 0x51, 0xDB, 0xAE, 0xFA, 0x51, 0x88, 0x59
};

static const uint8_t vmp_iv[0x10] = {
	0xB3, 0x0F, 0xFE, 0xED, 0xB7, 0xDC, 0x5E, 0xB7, 0x13, 0x3D, 0xA6, 0x0D, 0x1B, 0x6B, 0x2C, 0xDC
};


static void XorWithByte(uint8_t* buf, uint8_t byte, int length)
{
	for (int i = 0; i < length; ++i)
		buf[i] ^= byte;
}

static void XorWithIv(uint8_t* buf, const uint8_t* Iv)
{
	// The block in AES is always 128bit no matter the key size
	for (uint8_t i = 0; i < 16; ++i)
		buf[i] ^= Iv[i];
}
 
static void generateHash(const uint8_t *input, uint8_t *dest, size_t sz)
{
	aes_context aes_ctx;
	sha1_context sha1_ctx;
	uint8_t salt[0x40];
	uint8_t work_buf[0x14];
	const uint8_t *salt_seed = input + VMP_SEED_OFFSET;

	memset(salt , 0, sizeof(salt));
	memset(&aes_ctx, 0, sizeof(aes_context));

	LOG("Signing VMP Memory Card File...");
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
	memset(dest, 0, 0x14);

	XorWithByte(salt, 0x36, sizeof(salt));

	memset(&sha1_ctx, 0, sizeof(sha1_context));
	sha1_starts(&sha1_ctx);
	sha1_update(&sha1_ctx, salt, sizeof(salt));
	sha1_update(&sha1_ctx, input, sz);
	sha1_finish(&sha1_ctx, work_buf);

	XorWithByte(salt, 0x6A, sizeof(salt));

	memset(&sha1_ctx, 0, sizeof(sha1_context));
	sha1_starts(&sha1_ctx);
	sha1_update(&sha1_ctx, salt, sizeof(salt));
	sha1_update(&sha1_ctx, work_buf, 0x14);
	sha1_finish(&sha1_ctx, dest);
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

	if (*(uint32_t*)input != VMP_MAGIC || sz != VMP_SIZE) {
		LOG("Not a VMP file");
		free(input);
		return 0;
	}
//	LOG("Old signature:");
//	dump_data(input+VMP_HASH_OFFSET, 20);

	generateHash(input, input + VMP_HASH_OFFSET, sz);

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

int ps1_mcr2vmp(const char* mcrfile, const char* dstName)
{
	uint32_t hdr[0x20];
	uint8_t *input;
	size_t sz;
	FILE *pf;

	if (read_buffer(mcrfile, &input, &sz) < 0) {
		LOG("Failed to open input file");
		return 0;
	}

	if (*(uint32_t*)input != MCR_MAGIC || sz != MC_SIZE) {
		LOG("Not a .mcr file");
		free(input);
		return 0;
	}

	pf = fopen(dstName, "wb");
	if (!pf) {
		LOG("Failed to open output file");
		free(input);
		return 0;
	}

	memset(hdr, 0, sizeof(hdr));
	memcpy(&hdr[3], "bucanero.com.ar", 0x10);
	hdr[0] = VMP_MAGIC;
	hdr[1] = MCR_OFFSET;

	fwrite(hdr, sizeof(hdr), 1, pf);
	fwrite(input, sz, 1, pf);
	fclose(pf);
	free(input);

	return vmp_resign(dstName);
}

int ps1_vmp2mcr(const char* vmpfile, const char* dstName)
{
	uint8_t *input;
	size_t sz;

	if (read_buffer(vmpfile, &input, &sz) < 0) {
		LOG("Failed to open input file");
		return 0;
	}

	if (*(uint32_t*)input != VMP_MAGIC || sz != VMP_SIZE) {
		LOG("Not a .VMP file");
		free(input);
		return 0;
	}

	if (write_buffer(dstName, input + 0x80, sz - 0x80) < 0) {
		LOG("Failed to open output file");
		free(input);
		return 0;
	}

	free(input);
	LOG("MCR exported: %s", dstName);

	return 1;
}
