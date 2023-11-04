#include <zip.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#define _UNIX
#include <unrar.h>
#include <un7zip.h>

#include "saves.h"
#include "common.h"

#define UNZIP_BUF_SIZE 0x20000

static inline uint64_t min64(uint64_t a, uint64_t b)
{
    return a < b ? a : b;
}

static void walk_zip_directory(const char* startdir, const char* inputdir, struct zip *zipper)
{
	char fullname[256];	
	struct dirent *dirp;
	int len = strlen(startdir) + 1;
	DIR *dp = opendir(inputdir);

	if (!dp) {
		LOG("Failed to open input directory: '%s'", inputdir);
		return;
	}

	if (strlen(inputdir) > len)
	{
		LOG("Adding folder '%s'", inputdir+len);
		if (zip_add_dir(zipper, inputdir+len) < 0)
		{
			LOG("Failed to add directory to zip: %s", inputdir);
			return;
		}
	}

	while ((dirp = readdir(dp)) != NULL) {
		if ((strcmp(dirp->d_name, ".")  != 0) && (strcmp(dirp->d_name, "..") != 0)) {
			snprintf(fullname, sizeof(fullname), "%s%s", inputdir, dirp->d_name);

			if (dirp->d_stat.st_mode & SCE_S_IFDIR) {
				strcat(fullname, "/");
				walk_zip_directory(startdir, fullname, zipper);
			} else {
				struct zip_source *source = zip_source_file(zipper, fullname, 0, 0);
				if (!source) {
					LOG("Failed to source file to zip: %s", fullname);
					continue;
				}
				LOG("Adding file '%s'", fullname+len);
				zip_int64_t zidx = zip_add(zipper, &fullname[len], source);
				if (zidx < 0) {
					zip_source_free(source);
					LOG("Failed to add file to zip: %s", fullname);
					continue;
				}
				zip_file_set_external_attributes(zipper, zidx, 0, ZIP_OPSYS_UNIX, (zip_uint32_t)(0100644) << 16);
			}
		}
	}
	closedir(dp);
}

static int zip_dir_ex(struct zip* archive, const char* basedir, const char* inputdir, const char* output_filename)
{
    if (!archive) {
        LOG("Failed to open output file '%s'", output_filename);
        return 0;
    }

    LOG("Zipping <%s> to %s...", inputdir, output_filename);
    walk_zip_directory(basedir, inputdir, archive);

    return (zip_close(archive) == ZIP_ER_OK);
}

int zip_directory(const char* basedir, const char* inputdir, const char* output_filename)
{
	unlink_secure(output_filename);
	struct zip* archive = zip_open(output_filename, ZIP_CREATE | ZIP_EXCL, NULL);

	return (zip_dir_ex(archive, basedir, inputdir, output_filename));
}

int zip_append_directory(const char* basedir, const char* inputdir, const char* output_filename)
{
	int ret;
	size_t fsz;
	zip_int64_t sz;
	uint8_t *fbuf, *outbuf;
	struct zip* archive;
	zip_source_t *zsmem;
	
	if (read_buffer(output_filename, &fbuf, &fsz) < 0)
		return 0;

	zsmem = zip_source_buffer_create(fbuf, fsz, 0, NULL);
	zip_source_keep(zsmem); // zip_close will free source, but we want to use its content
	archive = zip_open_from_source(zsmem, 0, NULL);

	LOG("Append Zip <%s> to %s...", inputdir, output_filename);
	if (!archive) {
		zip_source_free(zsmem);
		free(fbuf);
		LOG("Failed to open output file '%s'", output_filename);
		return 0;
	}

	walk_zip_directory(basedir, inputdir, archive);
	ret = zip_close(archive);

	zip_source_open(zsmem);
	zip_source_seek(zsmem, 0, SEEK_END);
	sz = zip_source_tell(zsmem);
	zip_source_seek(zsmem, 0, SEEK_SET);
	outbuf = malloc(sz);

	zip_source_read(zsmem, outbuf, sz);
	zip_source_close(zsmem);
	zip_source_free(zsmem);
	free(fbuf);

	if (write_buffer(output_filename, outbuf, sz) < 0)
		ret = ZIP_ER_WRITE;

	free(outbuf);

	return (ret == ZIP_ER_OK);
}

static int on_extract_entry(const char *filename, void *arg)
{
	uint64_t* progress = (uint64_t*) arg;

    LOG("Extracted: %s", filename);
    update_progress_bar(++progress[0], progress[1], filename);

    return 0;
}

int extract_zip(const char* zip_file, const char* dest_path)
{
	char path[256];
	uint8_t* buffer;
	struct zip* archive = zip_open(zip_file, ZIP_RDONLY | ZIP_CHECKCONS, NULL);
	int files = zip_get_num_files(archive);

	if (files <= 0) {
		LOG("Empty ZIP file.");
		zip_close(archive);
		return 0;
	}

	buffer = malloc(UNZIP_BUF_SIZE);
	if (!buffer)
		return 0;

	init_progress_bar("Extracting files...");

	LOG("Extracting %s to <%s>...", zip_file, dest_path);

	for (int i = 0; i < files; i++) {
		const char* filename = zip_get_name(archive, i, 0);

		update_progress_bar(i+1, files, filename);
		LOG("Unzip [%d/%d] '%s'...", i+1, files, filename);

		if (!filename)
			continue;

		if (filename[0] == '/')
			filename++;

		snprintf(path, sizeof(path)-1, "%s%s", dest_path, filename);
		mkdirs(path);

		if (filename[strlen(filename) - 1] == '/')
			continue;

		struct zip_stat st;
		if (zip_stat_index(archive, i, 0, &st)) {
			LOG("Unable to access file %s in zip.", filename);
			continue;
		}
		struct zip_file* zfd = zip_fopen_index(archive, i, 0);
		if (!zfd) {
			LOG("Unable to open file %s in zip.", filename);
			continue;
		}

		FILE* tfd = fopen(path, "wb");
		if(!tfd) {
			free(buffer);
			zip_fclose(zfd);
			zip_close(archive);
			end_progress_bar();
			LOG("Error opening temporary file '%s'.", path);
			return 0;
		}

		uint64_t pos = 0, count;
		while (pos < st.size) {
			count = min64(UNZIP_BUF_SIZE, st.size - pos);
			if (zip_fread(zfd, buffer, count) != count) {
				free(buffer);
				fclose(tfd);
				zip_fclose(zfd);
				zip_close(archive);
				end_progress_bar();
				LOG("Error reading from zip.");
				return 0;
			}

			fwrite(buffer, count, 1, tfd);
			pos += count;
		}

		zip_fclose(zfd);
		fclose(tfd);
	}

	if (archive) {
		zip_close(archive);
	}

	end_progress_bar();
	free(buffer);

	return files;
}

static void callback_7z(const char* fileName, unsigned long fileSize, unsigned fileNum, unsigned numFiles)
{
    LOG("Extracted: %s (%ld bytes)", fileName, fileSize);
    update_progress_bar(fileNum, numFiles, fileName);
}

int extract_7zip(const char* fpath, const char* dest_path)
{
	int ret;

	LOG("Extracting 7-Zip (%s) to <%s>...", fpath, dest_path);
	init_progress_bar("Extracting files...");

	// Extract 7-Zip archive contents
	ret = Extract7zFileEx(fpath, dest_path, &callback_7z, 0x10000);
	end_progress_bar();

	return (ret == SUCCESS);
}

int extract_rar(const char* rarFilePath, const char* dstPath)
{
	int err = 0;
	HANDLE hArcData; //Archive Handle
	struct RAROpenArchiveDataEx rarOpenArchiveData;
	struct RARHeaderDataEx rarHeaderData;

	memset(&rarOpenArchiveData, 0, sizeof(rarOpenArchiveData));
	memset(&rarHeaderData, 0, sizeof(rarHeaderData));
	rarOpenArchiveData.ArcName = (char*) rarFilePath;
	rarOpenArchiveData.OpenMode = RAR_OM_EXTRACT;

	hArcData = RAROpenArchiveEx(&rarOpenArchiveData);
	if (rarOpenArchiveData.OpenResult != ERAR_SUCCESS)
	{
		LOG("OpenArchive '%s' Failed!", rarOpenArchiveData.ArcName);
		return 0;
	}

	LOG("UnRAR Extract %s to '%s'...", rarFilePath, dstPath);
	init_progress_bar("Extracting files...");

	while (RARReadHeaderEx(hArcData, &rarHeaderData) == ERAR_SUCCESS)
	{
		LOG("Extracting '%s' (%ld bytes)", rarHeaderData.FileName, rarHeaderData.UnpSize + (((uint64_t)rarHeaderData.UnpSizeHigh) << 32));
		update_progress_bar(0, 1, rarHeaderData.FileName);

		if (RARProcessFile(hArcData, RAR_EXTRACT, (char*) dstPath, NULL) != ERAR_SUCCESS)
		{
			err++;
			LOG("ERROR: UnRAR Extract Failed!");
			continue;
		}
		update_progress_bar(1, 1, rarHeaderData.FileName);
	}
	end_progress_bar();

	RARCloseArchive(hArcData);
	return (err == ERAR_SUCCESS);
}
