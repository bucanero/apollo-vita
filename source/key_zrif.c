//
// based on https://github.com/st4rk/PkgDecrypt
//
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>
#include <psp2/npdrm.h>

#include "utils.h"

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 2

//---------------------------------------------
// From NoNpDRM by theFlow
//---------------------------------------------
#define FAKE_AID 0x0123456789ABCDEFLL

/*
    Deflate-Inflate convenience methods for key compression.
    Include dictionary with set of strings for more efficient packing
*/

#define ZRIF_DICT_SIZE 1024
static const unsigned char g_dict[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 48, 48, 48, 48, 57, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 48, 48,
    48, 48, 54, 48, 48, 48, 48, 55, 48, 48, 48, 48, 56, 0, 48, 48, 48, 48, 51, 48, 48, 48, 48, 52, 48, 48, 48, 48,
    53, 48, 95, 48, 48, 45, 65, 68, 68, 67, 79, 78, 84, 48, 48, 48, 48, 50, 45, 80, 67, 83, 71, 48, 48, 48, 48,
    48, 48, 48, 48, 48, 48, 49, 45, 80, 67, 83, 69, 48, 48, 48, 45, 80, 67, 83, 70, 48, 48, 48, 45, 80, 67, 83,
    67, 48, 48, 48, 45, 80, 67, 83, 68, 48, 48, 48, 45, 80, 67, 83, 65, 48, 48, 48, 45, 80, 67, 83, 66, 48, 48,
    48, 0, 1, 0, 1, 0, 1, 0, 2, 239, 205, 171, 137, 103, 69, 35, 1, 0};

static int deflateKey( uint8_t *license, uint8_t *out, size_t out_size ) {
    int result = 0;
    z_streamp z_str = malloc( sizeof( z_stream ) );
    z_str->next_in = license;
    z_str->avail_in = 512;
    z_str->next_out = out;
    z_str->avail_out = out_size;
    z_str->zalloc = Z_NULL;
    z_str->zfree = Z_NULL;
    z_str->opaque = Z_NULL;

    if ( deflateInit2( z_str, 9, Z_DEFLATED, 10, 8, Z_DEFAULT_STRATEGY ) == Z_OK ) {
        if ( deflateSetDictionary( z_str, g_dict, ZRIF_DICT_SIZE ) == Z_OK ) {
            if ( deflate( z_str, Z_FINISH ) == Z_STREAM_END ) {
                deflateEnd( z_str );
                result = out_size - z_str->avail_out;
            } else {
                result = 0x80040003;
            }
        } else {
            result = 0x80040002;
        }
    } else {
        result = 0x80040001;
    }

    free( z_str );
    return result;
}

static int inflateKey( uint8_t *in, size_t in_size, uint8_t *license ) {
    int result = 0;
    z_streamp z_str = malloc( sizeof( z_stream ) );
    z_str->next_in = in;
    z_str->avail_in = in_size;
    z_str->next_out = license;
    z_str->avail_out = 512;
    z_str->zalloc = Z_NULL;
    z_str->zfree = Z_NULL;
    z_str->opaque = Z_NULL;

    if ( inflateInit2( z_str, 10 ) == Z_OK ) {
        if ( inflate( z_str, Z_NO_FLUSH ) == Z_NEED_DICT ) {
            if ( inflateSetDictionary( z_str, g_dict, ZRIF_DICT_SIZE ) == Z_OK ) {
                if ( inflate( z_str, Z_FINISH ) == Z_STREAM_END ) {
                    inflateEnd( z_str );
                    result = 512 - z_str->avail_out;
                } else {
                    result = 0x80040003;
                }
            } else {
                result = 0x80040002;
            }
        } else {
            result = 0x80040010;
        }
    } else {
        result = 0x80040001;
    }

    free( z_str );
    return result;
}

int make_key_zrif(const char *rif_path, const char *out_path) {
    int len;
    uint8_t key[512];
    uint8_t out[512];

    if ( read_file(rif_path, key, sizeof(key)) < 0 ) {
        LOG( "Error: %s is not a valid (or supported) license key (size mismatch).", rif_path );
        return 0;
    }

    LOG("make_key - zRIF generator, version %d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    SceNpDrmLicense *license = (SceNpDrmLicense *) key;

    //Check if it is a NoNpDRM license
    if ( license->account_id != FAKE_AID ) {
        LOG( "Warning: %s may be not a valid NoNpDRM fake license.", rif_path );
        license->account_id = FAKE_AID;
    }

    memset(out, 0, sizeof(out));
    if ( ( len = deflateKey(key, out, sizeof(out)) ) < 0 ) {
        LOG( "Error: failed to compress %s", rif_path );
        return 0;
    }
    LOG("Compressed key to %d bytes.", len);

    //Align len to 3 byte block to avoid padding by base64
    if ( ( len % 3 ) > 0 ) len += 3 - ( len % 3 );

    //Everything was ok, now encode binary buffer into base64 string
    char *zrif = dbg_base64_encode(out, len);
    if (!zrif) {
        LOG("Error: failed to encode zRIF");
        return 0;
    }

    snprintf(out, sizeof(out), "%s%s.zrif", out_path, license->content_id);
    if (write_buffer(out, zrif, strlen(zrif)) < 0) {
        LOG("Error: failed to write %s", out);
        free(zrif);
        return 0;
    }

    LOG("%s:\n\tContent ID: %s\n\tLicense: %s", out, license->content_id, zrif);
    free(zrif);

    return 1;
}
