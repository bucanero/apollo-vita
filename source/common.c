#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <dirent.h>
#include <zlib.h>
#include <psp2/apputil.h>
#include <psp2/system_param.h>

#include "types.h"
#include "common.h"

#define TMP_BUFF_SIZE 0x20000

//----------------------------------------
//String Utils
//----------------------------------------
int is_char_integer(char c)
{
	if (c >= '0' && c <= '9')
		return SUCCESS;
	return FAILED;
}

int is_char_letter(char c)
{
	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
		return SUCCESS;
	return FAILED;
}

char * safe_strncpy(char *dst, const char* src, size_t size)
{
    strncpy(dst, src, size);
    dst[size - 1] = '\0';
    return dst;
}

char * rstrip(char *s)
{
    char *p = s + strlen(s);
    while (p > s && isspace(*--p))
        *p = '\0';
    return s;
}

char * lskip(const char *s)
{
    while (*s != '\0' && isspace(*s))
        ++s;
    return (char *)s;
}

//----------------------------------------
//FILE UTILS
//----------------------------------------

int file_exists(const char *path)
{
    struct stat sb;
    if ((stat(path, &sb) == 0) && sb.st_mode & S_IFREG) {
        return SUCCESS;
    }
    return FAILED;
}

int dir_exists(const char *path)
{
    struct stat sb;
    if ((stat(path, &sb) == 0) && sb.st_mode & S_IFDIR) {
        return SUCCESS;
    }
    return FAILED;
}

int unlink_secure(const char *path)
{   
    if(file_exists(path)==SUCCESS)
    {
        chmod(path, 0777);
		return remove(path);
    }
    return FAILED;
}

/*
* Creates all the directories in the provided path. (can include a filename)
* (directory must end with '/')
*/
int mkdirs(const char* dir)
{
    char path[256];
    snprintf(path, sizeof(path), "%s", dir);

    char* ptr = strrchr(path, '/');
    *ptr = 0;
    ptr = path;
    ptr++;
    while (*ptr)
    {
        while (*ptr && *ptr != '/')
            ptr++;

        char last = *ptr;
        *ptr = 0;

        if (dir_exists(path) == FAILED)
        {
            if (mkdir(path, 0777) < 0)
                return FAILED;
            else
                chmod(path, 0777);
        }
        
        *ptr++ = last;
        if (last == 0)
            break;

    }

    return SUCCESS;
}

int copy_file(const char* input, const char* output)
{
    size_t read, written;
    FILE *fd, *fd2;

    if (mkdirs(output) != SUCCESS)
        return FAILED;

    if((fd = fopen(input, "rb")) == NULL)
        return FAILED;

    if((fd2 = fopen(output, "wb")) == NULL)
    {
        fclose(fd);
        return FAILED;
    }

    char* buffer = malloc(TMP_BUFF_SIZE);

    if (!buffer)
        return FAILED;

    do
    {
        read = fread(buffer, 1, TMP_BUFF_SIZE, fd);
        written = fwrite(buffer, 1, read, fd2);
    }
    while ((read == written) && (read == TMP_BUFF_SIZE));

    free(buffer);
    fclose(fd);
    fclose(fd2);
    chmod(output, 0777);

    return (read - written);
}

uint32_t file_crc32(const char* input)
{
    Bytef *buffer;
    uLong crc = crc32(0L, Z_NULL, 0);
    size_t read;

    FILE* in = fopen(input, "rb");
    
    if (!in)
        return FAILED;

    buffer = malloc(TMP_BUFF_SIZE);
    do
    {
        read = fread(buffer, 1, TMP_BUFF_SIZE, in);
        crc = crc32(crc, buffer, read);
    }
    while (read == TMP_BUFF_SIZE);

    free(buffer);
    fclose(in);

    return crc;
}

int copy_directory(const char* startdir, const char* inputdir, const char* outputdir)
{
    char fullname[256];
    char out_name[256];
    struct dirent *dirp;
    int len = strlen(startdir);
    DIR *dp = opendir(inputdir);

    if (!dp) {
        return FAILED;
    }

    while ((dirp = readdir(dp)) != NULL) {
        if ((strcmp(dirp->d_name, ".")  != 0) && (strcmp(dirp->d_name, "..") != 0)) {
            snprintf(fullname, sizeof(fullname), "%s%s", inputdir, dirp->d_name);

            if (dirp->d_stat.st_mode & SCE_S_IFDIR) {
                strcat(fullname, "/");
                if (copy_directory(startdir, fullname, outputdir) != SUCCESS) {
                    return FAILED;
                }
            } else {
                snprintf(out_name, sizeof(out_name), "%s%s", outputdir, &fullname[len]);
                if (copy_file(fullname, out_name) != SUCCESS) {
                    // skip keystone and sealedkey files error
                    // when extracting to the mounted save path (workaround)
                    if (strncmp(outputdir, "ux0:user/00/savedata/", 21) != 0 || 
                        !(strcmp(dirp->d_name, "keystone") == 0 || strcmp(dirp->d_name, "sealedkey") == 0))
                        return FAILED;
                }
            }
        }
    }
    closedir(dp);

    return SUCCESS;
}

int clean_directory(const char* inputdir, const char* filter)
{
	DIR *d;
	struct dirent *dir;
	char dataPath[256];

	d = opendir(inputdir);
	if (!d)
		return FAILED;

	while ((dir = readdir(d)) != NULL)
	{
		if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0 && strstr(dir->d_name, filter) != NULL)
		{
			snprintf(dataPath, sizeof(dataPath), "%s" "%s", inputdir, dir->d_name);
			unlink_secure(dataPath);
		}
	}
	closedir(d);

    return SUCCESS;
}

const char * get_user_language(void)
{
    int language;

    if(sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, &language) < 0)
        return "en";

    switch (language)
    {
    case SCE_SYSTEM_PARAM_LANG_JAPANESE:             //  0   Japanese
        return "ja";

    case SCE_SYSTEM_PARAM_LANG_ENGLISH_US:           //  1   English (United States)
    case SCE_SYSTEM_PARAM_LANG_ENGLISH_GB:           // 18   English (United Kingdom)
        return "en";

    case SCE_SYSTEM_PARAM_LANG_FRENCH:               //  2   French
        return "fr";

    case SCE_SYSTEM_PARAM_LANG_SPANISH:              //  3   Spanish
        return "es";

    case SCE_SYSTEM_PARAM_LANG_GERMAN:               //  4   German
        return "de";

    case SCE_SYSTEM_PARAM_LANG_ITALIAN:              //  5   Italian
        return "it";

    case SCE_SYSTEM_PARAM_LANG_DUTCH:                //  6   Dutch
        return "nl";

    case SCE_SYSTEM_PARAM_LANG_RUSSIAN:              //  8   Russian
        return "ru";

    case SCE_SYSTEM_PARAM_LANG_KOREAN:               //  9   Korean
        return "ko";

    case SCE_SYSTEM_PARAM_LANG_CHINESE_T:            // 10   Chinese (traditional)
    case SCE_SYSTEM_PARAM_LANG_CHINESE_S:            // 11   Chinese (simplified)
        return "zh";

    case SCE_SYSTEM_PARAM_LANG_FINNISH:              // 12   Finnish
        return "fi";

    case SCE_SYSTEM_PARAM_LANG_SWEDISH:              // 13   Swedish
        return "sv";

    case SCE_SYSTEM_PARAM_LANG_DANISH:               // 14   Danish
        return "da";

    case SCE_SYSTEM_PARAM_LANG_NORWEGIAN:            // 15   Norwegian
        return "no";

    case SCE_SYSTEM_PARAM_LANG_POLISH:               // 16   Polish
        return "pl";

    case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT:        //  7   Portuguese (Portugal)
    case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR:        // 17   Portuguese (Brazil)
        return "pt";

    case SCE_SYSTEM_PARAM_LANG_TURKISH:              // 19   Turkish
        return "tr";

    default:
        break;
    }

    return "en";
}
