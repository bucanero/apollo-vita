#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <polarssl/md5.h>
#include <psp2/net/netctl.h>
#include <mini18n.h>

#include "saves.h"
#include "menu.h"
#include "common.h"
#include "utils.h"
#include "sfo.h"
#include "ps1card.h"
#include "svpng.h"

static char host_buf[256];

static int _set_dest_path(char* path, int dest, const char* folder)
{
	switch (dest)
	{
	case STORAGE_UMA0:
		sprintf(path, "%s%s", UMA0_PATH, folder);
		break;

	case STORAGE_IMC0:
		sprintf(path, "%s%s", IMC0_PATH, folder);
		break;

	case STORAGE_XMC0:
		sprintf(path, "%s%s", XMC0_PATH, folder);
		break;

	case STORAGE_UX0:
		sprintf(path, "%s%s", UX0_PATH, folder);
		break;

	default:
		path[0] = 0;
		return 0;
	}

	return 1;
}

static void downloadSave(const save_entry_t* entry, const char* file, int dst)
{
	char path[256];

	_set_dest_path(path, dst, (entry->flags & SAVE_FLAG_PS1) ? PS1_SAVES_PATH_USB : PSV_SAVES_PATH_USB);
	if (mkdirs(path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Export folder is not available:"), path);
		return;
	}

	if (!http_download(entry->path, file, APOLLO_LOCAL_CACHE "tmpsave.zip", 1))
	{
		show_message("%s\n%s%s", _("Error downloading save game from:"), entry->path, file);
		return;
	}

	if (extract_zip(APOLLO_LOCAL_CACHE "tmpsave.zip", path))
		show_message("%s\n%s", _("Save game successfully downloaded to:"), path);
	else
		show_message(_("Error extracting save game!"));

	unlink_secure(APOLLO_LOCAL_CACHE "tmpsave.zip");
}

static uint32_t get_filename_id(const char* dir, const char* title_id)
{
	char path[128];
	uint32_t tid = 0;

	do
	{
		tid++;
		snprintf(path, sizeof(path), "%s%s-%08d.zip", dir, title_id, tid);
	}
	while (file_exists(path) == SUCCESS);

	return tid;
}

static void zipSave(const save_entry_t* entry, int dst)
{
	char exp_path[256];
	char export_file[256];
	char* tmp;
	uint32_t fid;
	int ret;

	_set_dest_path(exp_path, dst, EXPORT_PATH);
	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Export folder is not available:"), exp_path);
		return;
	}

	init_loading_screen("Exporting save game...");

	fid = get_filename_id(exp_path, entry->title_id);
	snprintf(export_file, sizeof(export_file), "%s%s-%08d.zip", exp_path, entry->title_id, fid);

	tmp = strdup(entry->path);
	*strrchr(tmp, '/') = 0;
	*strrchr(tmp, '/') = 0;

	ret = zip_directory(tmp, entry->path, export_file);
	free(tmp);

	if (ret)
	{
		snprintf(export_file, sizeof(export_file), "%s%08x.txt", exp_path, apollo_config.user_id);
		FILE* f = fopen(export_file, "a");
		if (f)
		{
			fprintf(f, "%s-%08d.zip=%s\n", entry->title_id, fid, entry->name);
			fclose(f);
		}

		sprintf(export_file, "%s%s", exp_path, OWNER_XML_FILE);
		save_xml_owner(export_file);
	}

	stop_loading_screen();
	if (!ret)
	{
		show_message("%s\n%s", _("Error! Can't export save game to:"), exp_path);
		return;
	}

	show_message("%s\n%s%s-%08d.zip", _("Zip file successfully saved to:"), exp_path, entry->title_id, fid);
}

static void copySave(const save_entry_t* save, int dev)
{
	int ret;
	char* copy_path;
	char exp_path[256];

	_set_dest_path(exp_path, dev, PSV_SAVES_PATH_USB);
	if (strncmp(save->path, exp_path, strlen(exp_path)) == 0)
	{
		show_message("%s\n%s", _("Copy operation cancelled!"), _("Same source and destination."));
		return;
	}

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Export folder is not available:"), exp_path);
		return;
	}

	init_loading_screen("Copying files...");

	asprintf(&copy_path, "%s%s_%s/", exp_path, save->title_id, save->dir_name);

	LOG("Copying <%s> to %s...", save->path, copy_path);
	ret = (copy_directory(save->path, save->path, copy_path) == SUCCESS);

	free(copy_path);

	stop_loading_screen();
	if (ret)
		show_message("%s\n%s", _("Files successfully copied to:"), exp_path);
	else
		show_message("%s\n%s", _("Error! Can't copy save game to:"), exp_path);
}

static int get_psp_save_key(const save_entry_t* entry, uint8_t* key)
{
	FILE* fp;
	char path[256];

	snprintf(path, sizeof(path), "ux0:pspemu/PSP/SAVEPLAIN/%s/%s.bin", entry->dir_name, entry->title_id);
	if (read_psp_game_key(path, key))
		return 1;

	snprintf(path, sizeof(path), "ux0:pspemu/PSP/SAVEPLAIN/%s/%s.bin", entry->title_id, entry->title_id);
	if (read_psp_game_key(path, key))
		return 1;

	// SGKeyDumper 1.5+ support
	snprintf(path, sizeof(path), "ux0:pspemu/PSP/GAME/SED/gamekey/%s.bin", entry->title_id);
	if (read_psp_game_key(path, key))
		return 1;

	snprintf(path, sizeof(path), APOLLO_DATA_PATH "gamekeys.txt");
	if ((fp = fopen(path, "r")) == NULL)
		return 0;

	while(fgets(path, sizeof(path), fp))
	{
		char *ptr = strchr(path, '=');

		if (!ptr || path[0] == ';')
			continue;

		*ptr++ = 0;
		ptr[32] = 0;
		if (strncasecmp(entry->dir_name, path, strlen(path)) == 0)
		{
			LOG("[DB] %s Key found: %s", path, ptr);
			ptr = x_to_u8_buffer(ptr);
			if (!ptr)
				continue;

			memcpy(key, ptr, 16);
			free(ptr);
			fclose(fp);

			return 1;
		}
	}
	fclose(fp);

	return 0;
}

static int _copy_save_hdd(const save_entry_t* save)
{
	int ret;
	char copy_path[256];
	save_entry_t entry = {
		.title_id = save->title_id,
		.dir_name = save->dir_name,
		.path = copy_path,
	};

	snprintf(copy_path, sizeof(copy_path), APOLLO_SANDBOX_PATH, save->dir_name);
	if (!vita_SaveMount(&entry))
		return 0;

	LOG("Copying <%s> to %s...", save->path, copy_path);
	ret = (copy_directory(save->path, save->path, copy_path) == SUCCESS);

	vita_SaveUmount();
	return ret;
}

static int _copy_save_psp(const save_entry_t* save)
{
	char copy_path[256];

	snprintf(copy_path, sizeof(copy_path), PSP_SAVES_PATH_HDD "%s/", save->dir_name);

	LOG("Copying <%s> to %s...", save->path, copy_path);
	return (copy_directory(save->path, save->path, copy_path) == SUCCESS);
}

static void downloadSaveHDD(const save_entry_t* entry, const char* file)
{
	int ret;
	save_entry_t save;
	char path[256];
	char titleid[0x10];
	char dirname[0x30];
	sfo_context_t* sfo;

	if (!http_download(entry->path, file, APOLLO_LOCAL_CACHE "tmpsave.zip", 1))
	{
		show_message("%s\n%s%s", _("Error downloading save game from:"), entry->path, file);
		return;
	}

	sfo = sfo_alloc();
	if (!extract_sfo(APOLLO_LOCAL_CACHE "tmpsave.zip", APOLLO_LOCAL_CACHE) ||
		sfo_read(sfo, APOLLO_LOCAL_CACHE "param.sfo") < 0)
	{
		LOG("Unable to read SFO from '%s'", APOLLO_LOCAL_CACHE);
		sfo_free(sfo);

		show_message(_("Error extracting save game!"));
		return;
	}

	memset(&save, 0, sizeof(save_entry_t));
	strncpy(dirname, (entry->flags & SAVE_FLAG_PSP) ?
			(char*) sfo_get_param_value(sfo, "SAVEDATA_DIRECTORY") :
			(char*) sfo_get_param_value(sfo, "PARENT_DIRECTORY") + 1, sizeof(dirname));
	snprintf(titleid, sizeof(titleid), "%.9s", dirname);
	save.path = path;
	save.title_id = titleid;
	save.dir_name = dirname;
	sfo_free(sfo);

	if (entry->flags & SAVE_FLAG_PSV)
	{
		snprintf(path, sizeof(path), APOLLO_SANDBOX_PATH, save.dir_name);
		if (dir_exists(save.path) != SUCCESS)
		{
			show_message("%s\n%s", _("Error! save game folder is not available:"), save.path);
			return;
		}

		if (!show_dialog(DIALOG_TYPE_YESNO, "%s\n%s\n\n%s", _("Save game already exists:"), save.dir_name, _("Overwrite?")))
			return;

		if (!vita_SaveMount(&save))
		{
			show_message(_("Error mounting save game folder!"));
			return;
		}

		snprintf(path, sizeof(path), APOLLO_SANDBOX_PATH, "~");
		*strrchr(path, '~') = 0;
	}
	else
	{
		snprintf(path, sizeof(path), PSP_SAVES_PATH_HDD "%s/", save.dir_name);
		if ((dir_exists(save.path) == SUCCESS) &&
			!show_dialog(DIALOG_TYPE_YESNO, "%s\n%s\n\n%s", _("Save game already exists:"), save.dir_name, _("Overwrite?")))
			return;

		strncpy(path, PSP_SAVES_PATH_HDD, sizeof(path));
	}

	ret = extract_zip(APOLLO_LOCAL_CACHE "tmpsave.zip", path);
	unlink_secure(APOLLO_LOCAL_CACHE "tmpsave.zip");

	if (entry->flags & SAVE_FLAG_PSV)
		vita_SaveUmount();

	if (ret)
		show_message("%s\n%s%s", _("Save game successfully downloaded to:"), path, save.dir_name);
	else
		show_message(_("Error extracting save game!"));
}

static void copySaveHDD(const save_entry_t* save)
{
	//source save is already on HDD
	if (save->flags & SAVE_FLAG_HDD)
	{
		show_message("%s\n%s", _("Copy operation cancelled!"), _("Same source and destination."));
		return;
	}

	init_loading_screen("Copying save game...");
	int ret = (save->flags & SAVE_FLAG_PSP) ? _copy_save_psp(save) : _copy_save_hdd(save);
	stop_loading_screen();

	if (ret)
		show_message("%s\n%s/%s", _("Files successfully copied to:"), save->title_id, save->dir_name);
	else
		show_message("%s\n%s/%s", _("Error! Can't copy Save-game folder:"), save->title_id, save->dir_name);
}

static void copyAllSavesHDD(const save_entry_t* save, int all)
{
	int done = 0, err_count = 0;
	list_node_t *node;
	save_entry_t *item;
	uint64_t progress = 0;
	list_t *list = ((void**)save->dir_name)[0];

	init_progress_bar("Copying all saves...");

	LOG("Copying all saves from '%s' to HDD...", save->path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (!all && !(item->flags & SAVE_FLAG_SELECTED))
			continue;

		if (item->type == FILE_TYPE_PSV && !(item->flags & SAVE_FLAG_LOCKED))
			(_copy_save_hdd(item) ? done++ : err_count++);

		if (item->type == FILE_TYPE_PSP)
			(_copy_save_psp(item) ? done++ : err_count++);
	}

	end_progress_bar();

	show_message("%d/%d %s", done, done+err_count, _("Saves copied to Internal Storage"));
}

static void extractArchive(const char* file_path)
{
	int ret = 0;
	char exp_path[256];

	strncpy(exp_path, file_path, sizeof(exp_path));
	*strrchr(exp_path, '.') = 0;

	switch (strrchr(file_path, '.')[1])
	{
	case 'z':
	case 'Z':
		/* ZIP */
		strcat(exp_path, "/");
		ret = extract_zip(file_path, exp_path);
		break;

	case 'r':
	case 'R':
		/* RAR */
		ret = extract_rar(file_path, exp_path);
		break;

	case '7':
		/* 7-Zip */
		ret = extract_7zip(file_path, exp_path);
		break;

	default:
		break;
	}

	if (ret)
		show_message("%s\n%s", _("All files extracted to:"), exp_path);
	else
		show_message(_("Error: %s couldn't be extracted"), file_path);
}

static void exportFingerprint(const save_entry_t* save, int silent)
{
	char fpath[256];
	uint8_t buffer[0x40];

	snprintf(fpath, sizeof(fpath), "%ssce_sys/keystone", save->path);
	LOG("Reading '%s' ...", fpath);

	if (read_file(fpath, buffer, sizeof(buffer)) != SUCCESS)
	{
		if (!silent) show_message("%s\n%s", _("Error! Keystone file is not available:"), fpath);
		return;
	}

	snprintf(fpath, sizeof(fpath), APOLLO_PATH "fingerprints.txt");
	FILE *fp = fopen(fpath, "a");
	if (!fp)
	{
		if (!silent) show_message("%s\n%s", _("Error! Can't open file:"), fpath);
		return;
	}

	fprintf(fp, "%s=", save->title_id);
	for (size_t i = 0x20; i < 0x40; i++)
		fprintf(fp, "%02x", buffer[i]);

	fprintf(fp, "\n");
	fclose(fp);

	if (!silent) show_message("%s %s\n%s", save->title_id, _("fingerprint successfully saved to:"), fpath);
}

static void exportTrophyZip(const save_entry_t *trop, int dev)
{
	int ret;
	char exp_path[256];
	char trp_path[256];
	char* export_file;
	char* tmp;

	_set_dest_path(exp_path, dev, TROPHIES_PATH_USB);
	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Export folder is not available:"), exp_path);
		return;
	}

	init_loading_screen("Exporting Trophy...");

	asprintf(&export_file, "%strophy_%s.zip", exp_path, trop->title_id);
	snprintf(trp_path, sizeof(trp_path), TROPHY_PATH_HDD, apollo_config.user_id);

	tmp = strdup(trp_path);
	*strrchr(tmp, '/') = 0;
	ret = zip_directory(tmp, trop->path, export_file);

	snprintf(trp_path, sizeof(trp_path), TROPHY_PATH_HDD "conf/%s/", apollo_config.user_id, trop->title_id);
	ret &= zip_append_directory(tmp, trp_path, export_file);

	trp_path[1] = 'x';
	sprintf(tmp, "%.12s", trp_path);
	ret &= zip_append_directory(tmp, trp_path, export_file);

	sprintf(export_file, "%s%s", exp_path, OWNER_XML_FILE);
	save_xml_owner(export_file);

	free(export_file);
	free(tmp);

	stop_loading_screen();
	if (!ret)
	{
		show_message("%s %s", _("Failed to export Trophy Set"), trop->title_id);
		return;
	}

	show_message("%s\n%strophy_%s.zip", _("Trophy Set successfully exported to:"), exp_path, trop->title_id);
}

static void pspDumpKey(const save_entry_t* save)
{
	char fpath[256];
	uint8_t buffer[0x10];

	if (!get_psp_save_key(save, buffer))
	{
		show_message("%s\n%s/%s.bin", _("Error! Game Key file is not available:"), save->dir_name, save->title_id);
		return;
	}

	snprintf(fpath, sizeof(fpath), APOLLO_PATH "gamekeys.txt");
	FILE *fp = fopen(fpath, "a");
	if (!fp)
	{
		show_message("%s\n%s", _("Error! Can't open file:"), fpath);
		return;
	}

	fprintf(fp, "%s=", save->title_id);
	for (size_t i = 0; i < sizeof(buffer); i++)
		fprintf(fp, "%02X", buffer[i]);

	fprintf(fp, "\n");
	fclose(fp);

	show_message("%s %s\n%s", save->title_id, _("game key successfully saved to:"), fpath);
}

static void pspExportKey(const save_entry_t* save)
{
	char fpath[256];
	uint8_t buffer[0x10];

	if (!get_psp_save_key(save, buffer))
	{
		show_message("%s\n%s/%s.bin", _("Error! Game Key file is not available:"), save->dir_name, save->title_id);
		return;
	}

	snprintf(fpath, sizeof(fpath), APOLLO_USER_PATH "%s/%s.bin", apollo_config.user_id, save->dir_name, save->title_id);
	mkdirs(fpath);

	if (write_buffer(fpath, buffer, sizeof(buffer)) == SUCCESS)
		show_message("%s %s\n%s", save->title_id, _("game key successfully saved to:"), fpath);
	else
		show_message("%s\n%s", _("Error! Can't save file:"), fpath);
}

static void dumpAllFingerprints(const save_entry_t* save)
{
	uint64_t progress = 0;
	list_node_t *node;
	save_entry_t *item;
	list_t *list = ((void**)save->dir_name)[0];

	init_progress_bar("Dumping all fingerprints...");

	LOG("Dumping all fingerprints from '%s'...", save->path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (item->type != FILE_TYPE_PSV || (item->flags & SAVE_FLAG_LOCKED))
			continue;

		if (item->flags & SAVE_FLAG_PSV && item->flags & SAVE_FLAG_HDD && !vita_SaveMount(item))
			continue;

		exportFingerprint(item, 1);

		if (item->flags & SAVE_FLAG_PSV && item->flags & SAVE_FLAG_HDD)
			vita_SaveUmount();
	}

	end_progress_bar();
	show_message("%s\n%sfingerprints.txt", _("All fingerprints dumped to:"), APOLLO_PATH);
}

static void importTrophy(const char* path, const char* trop_dir)
{
	save_entry_t tmp;
	char src_path[256];
	char dst_path[256];

	memset(&tmp, 0, sizeof(save_entry_t));
	tmp.path = dst_path;
	tmp.dir_name = (char*)trop_dir;
	tmp.title_id = (char*)trop_dir;

	snprintf(src_path, sizeof(src_path), "%s%s/data/", path, trop_dir);
	snprintf(tmp.path, sizeof(dst_path), TROPHY_PATH_HDD "data/%s/", apollo_config.user_id, trop_dir);

	if (!vita_SaveMount(&tmp))
	{
		show_message("%s\n%s", _("Error! Trophy folder is not available:"), tmp.path);
		return;
	}

	init_loading_screen("Importing trophy...");

	LOG("Copying <%s> to %s...", src_path, dst_path);
	copy_directory(src_path, src_path, dst_path);

	snprintf(src_path, sizeof(src_path), "%s%s/conf/", path, trop_dir);
	snprintf(dst_path, sizeof(dst_path), TROPHY_PATH_HDD "conf/%s/", apollo_config.user_id, trop_dir);
	LOG("Copying <%s> to %s...", src_path, dst_path);
	copy_directory(src_path, src_path, dst_path);

	snprintf(src_path, sizeof(src_path), "%s%s/conf_ux0/", path, trop_dir);
	dst_path[1] = 'x';
	LOG("Copying <%s> to %s...", src_path, dst_path);
	copy_directory(src_path, src_path, dst_path);

	stop_loading_screen();
	vita_SaveUmount();

	show_message("%s %s\n" TROPHY_PATH_HDD, trop_dir, _("Trophy successfully copied to:"), apollo_config.user_id);
}

static void exportAllSavesVMC(const save_entry_t* save, int dev, int all)
{
	char outPath[256];
	int done = 0, err_count = 0;
	list_node_t *node;
	save_entry_t *item;
	uint64_t progress = 0;
	list_t *list = ((void**)save->dir_name)[0];

	init_progress_bar("Exporting all VMC saves...");
	_set_dest_path(outPath, dev, PS1_SAVES_PATH_USB);
	mkdirs(outPath);

	LOG("Exporting all saves from '%s' to %s...", save->path, outPath);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (!all && !(item->flags & SAVE_FLAG_SELECTED))
			continue;

		if (item->type == FILE_TYPE_PS1)
			(saveSingleSave(outPath, item->blocks, PS1SAVE_PSV) ? done++ : err_count++);
	}

	end_progress_bar();

	show_message("%d/%d %s\n%s", done, done+err_count, _("Saves exported to:"), outPath);
}

static void exportVmcSave(const save_entry_t* save, int type, int dst_id)
{
	int ret = 0;
	char outPath[256];
	struct tm t;

	_set_dest_path(outPath, dst_id, PS1_SAVES_PATH_USB);
	mkdirs(outPath);
	if (type != PS1SAVE_PSV)
	{
		// build file path
		gmtime_r(&(time_t){time(NULL)}, &t);
		sprintf(strrchr(outPath, '/'), "/%s_%d-%02d-%02d_%02d%02d%02d.%s", save->title_id,
			t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec,
			(type == PS1SAVE_MCS) ? "mcs" : "psx");
	}

	if (saveSingleSave(outPath, save->blocks, type))
		show_message("%s\n%s", _("Save successfully exported to:"), outPath);
	else
		show_message("%s\n%s", _("Error exporting save:"), save->path);
}

static int deleteSave(const save_entry_t* save)
{
	int ret = 0;

	if (!show_dialog(DIALOG_TYPE_YESNO, _("Do you want to delete %s?"), save->dir_name))
		return 0;

	if (save->flags & SAVE_FLAG_PSP)
	{
		clean_directory(save->path, "");
		ret = (remove(save->path) == SUCCESS);
	}
	else if (save->flags & SAVE_FLAG_PS1)
		ret = formatSave(save->blocks);

	if (ret)
		show_message("%s\n%s", _("Save successfully deleted:"), save->dir_name);
	else
		show_message("%s\n%s", _("Error! Couldn't delete save:"), save->dir_name);

	return ret;
}

static void copyKeystone(const save_entry_t* entry, int import)
{
	char path_data[256];
	char path_save[256];

	snprintf(path_save, sizeof(path_save), "%ssce_sys/keystone", entry->path);
	snprintf(path_data, sizeof(path_data), APOLLO_USER_PATH "%s/keystone", apollo_config.user_id, entry->title_id);
	mkdirs(path_data);

	LOG("Copy '%s' <-> '%s'...", path_save, path_data);

	if (copy_file(import ? path_data : path_save, import ? path_save : path_data) == SUCCESS)
		show_message("%s\n%s", _("Keystone successfully copied to:"), import ? path_save : path_data);
	else
		show_message(_("Error! Keystone couldn't be copied"));
}

static int webReqHandler(dWebRequest_t* req, dWebResponse_t* res, void* list)
{
	list_node_t *node;
	save_entry_t *item;

	// http://ps3-ip:8080/
	if (strcmp(req->resource, "/") == 0)
	{
		uint64_t hash[2];
		md5_context ctx;

		md5_starts(&ctx);
		for (node = list_head(list); (item = list_get(node)); node = list_next(node))
			md5_update(&ctx, (uint8_t*) item->name, strlen(item->name));

		md5_finish(&ctx, (uint8_t*) hash);
		asprintf(&res->data, APOLLO_LOCAL_CACHE "web%016llx%016llx.html", hash[0], hash[1]);

		if (file_exists(res->data) == SUCCESS)
			return 1;

		FILE* f = fopen(res->data, "w");
		if (!f)
			return 0;

		fprintf(f, "<html><head><meta charset=\"UTF-8\"><style>h1, h2 { font-family: arial; } img { display: none; } table { border-collapse: collapse; margin: 25px 0; font-size: 0.9em; font-family: sans-serif; min-width: 400px; box-shadow: 0 0 20px rgba(0, 0, 0, 0.15); } table thead tr { background-color: #009879; color: #ffffff; text-align: left; } table th, td { padding: 12px 15px; } table tbody tr { border-bottom: 1px solid #dddddd; } table tbody tr:nth-of-type(even) { background-color: #f3f3f3; } table tbody tr:last-of-type { border-bottom: 2px solid #009879; }</style>");
		fprintf(f, "<script language=\"javascript\">function show(sid,src){var im=document.getElementById('img'+sid);im.src=src;im.style.display='block';document.getElementById('btn'+sid).style.display='none';}</script>");
		fprintf(f, "<title>Apollo Save Tool</title></head><body><h1>.:: Apollo Save Tool</h1><h2>Index of %s</h2><table><thead><tr><th>Name</th><th>Icon</th><th>Title ID</th><th>Folder</th><th>Location</th></tr></thead><tbody>", selected_entry->path);

		int i = 0;
		for (node = list_head(list); (item = list_get(node)); node = list_next(node), i++)
		{
			if (item->type == FILE_TYPE_MENU || item->type == FILE_TYPE_VMC || !(item->flags & (SAVE_FLAG_PSV|SAVE_FLAG_PSP|SAVE_FLAG_PS1)) || item->flags & SAVE_FLAG_LOCKED)
				continue;

			fprintf(f, "<tr><td><a href=\"/zip/%08x/%s_%s.zip\">%s</a></td>", i, item->title_id, item->dir_name, item->name);
			fprintf(f, "<td><button type=\"button\" id=\"btn%d\" onclick=\"show(%d,'/", i, i, i);

			if (item->flags & SAVE_FLAG_PSV)
				fprintf(f, "PSV/%s/icon0.png')\">Show Icon</button><img width=\"128\" height=\"128", item->title_id);
			else
				fprintf(f, "icon/%08x/ICON0.PNG')\">Show Icon</button><img width=\"%d\" height=\"80", i, (item->flags & SAVE_FLAG_PSP) ? 144 : 80);

			fprintf(f, "\" id=\"img%d\" alt=\"%s\"></td>", i, item->name);
			fprintf(f, "<td>%s</td>", item->title_id);
			fprintf(f, "<td>%s</td>", item->dir_name);
			fprintf(f, "<td>%.4s</td></tr>", item->path);
		}

		fprintf(f, "</tbody></table></body></html>");
		fclose(f);
		return 1;
	}

	// http://vita-ip:8080/PSV/games.txt
	if (wildcard_match(req->resource, "/PS?/games.txt"))
	{
		asprintf(&res->data, "%s%.3s%s", APOLLO_LOCAL_CACHE, req->resource+1, "_games.txt");

		FILE* f = fopen(res->data, "w");
		if (!f)
			return 0;

		for (node = list_head(list); (item = list_get(node)); node = list_next(node))
		{
			if (item->type == FILE_TYPE_MENU ||
				(strncmp(req->resource+1, "PSV", 3) == 0 && !(item->flags & SAVE_FLAG_PSV)) ||
				(strncmp(req->resource+1, "PSP", 3) == 0 && !(item->flags & SAVE_FLAG_PSP)))
				continue;

			fprintf(f, "%s=%s\n", item->title_id, item->name);
		}

		fclose(f);
		return 1;
	}

	// http://vita-ip:8080/PSV/BLUS12345/saves.txt
	if (wildcard_match(req->resource, "/PS?/\?\?\?\?\?\?\?\?\?/saves.txt"))
	{
		asprintf(&res->data, "%sweb%.9s_saves.txt", APOLLO_LOCAL_CACHE, req->resource + 5);

		FILE* f = fopen(res->data, "w");
		if (!f)
			return 0;

		int i = 0;
		for (node = list_head(list); (item = list_get(node)); node = list_next(node), i++)
		{
			if (item->type == FILE_TYPE_MENU || !(item->flags & (SAVE_FLAG_PSV|SAVE_FLAG_PSP)) || strncmp(item->title_id, req->resource + 5, 9))
				continue;

			fprintf(f, "%08d.zip=(%s) %s\n", i, item->dir_name, item->name);
		}

		fclose(f);
		return 1;
	}

	// http://vita-ip:8080/PSV/BLUS12345/00000000.zip
	if (wildcard_match(req->resource, "/PS?/\?\?\?\?\?\?\?\?\?/*.zip"))
	{
		char *base;
		int id = 0;

		sscanf(req->resource + 15, "%08d", &id);
		item = list_get_item(list, id);

		if (item->flags & SAVE_FLAG_PSV && item->flags & SAVE_FLAG_HDD && !vita_SaveMount(item))
			return 0;

		asprintf(&res->data, "%s%s.zip", APOLLO_LOCAL_CACHE, item->dir_name);
		base = strdup(item->path);
		*strrchr(base, '/') = 0;
		*strrchr(base, '/') = 0;

		id = zip_directory(base, item->path, res->data);
		if (item->flags & SAVE_FLAG_PSV && item->flags & SAVE_FLAG_HDD)
			vita_SaveUmount();

		free(base);
		return id;
	}

	// http://ps3-ip:8080/zip/00000000/CUSA12345_DIR-NAME.zip
	if (wildcard_match(req->resource, "/zip/\?\?\?\?\?\?\?\?/\?\?\?\?\?\?\?\?\?_*.zip"))
	{
		char *base;
		int id = 0;

		asprintf(&res->data, "%s%s", APOLLO_LOCAL_CACHE, req->resource + 14);
		sscanf(req->resource + 5, "%08x", &id);
		item = list_get_item(list, id);

		if (item->flags & SAVE_FLAG_PSV && item->flags & SAVE_FLAG_HDD && !vita_SaveMount(item))
			return 0;

		base = strdup(item->path);
		*strrchr(base, '/') = 0;
		*strrchr(base, '/') = 0;

		id = zip_directory(base, item->path, res->data);
		if (item->flags & SAVE_FLAG_PSV && item->flags & SAVE_FLAG_HDD)
			vita_SaveUmount();

		free(base);
		return id;
	}

	// http://vita-ip:8080/PSP/BLUS12345/ICON0.PNG
	if (wildcard_match(req->resource, "/PSP/\?\?\?\?\?\?\?\?\?/ICON0.PNG"))
	{
		for (node = list_head(list); (item = list_get(node)); node = list_next(node))
		{
			if (item->type == FILE_TYPE_MENU || !(item->flags & SAVE_FLAG_PSP) || strncmp(item->title_id, req->resource + 5, 9))
				continue;

			asprintf(&res->data, "%sICON0.PNG", item->path);
			return (file_exists(res->data) == SUCCESS);
		}

		return 0;
	}

	// http://vita-ip:8080/icon/00000000/ICON0.PNG
	if (wildcard_match(req->resource, "/icon/\?\?\?\?\?\?\?\?/ICON0.PNG"))
	{
		int id = 0;

		sscanf(req->resource + 6, "%08x", &id);
		item = list_get_item(list, id);
		asprintf(&res->data, "%sICON0.PNG", item->path);

		return (file_exists(res->data) == SUCCESS);
	}

	// http://vita-ip:8080/PSV/PCSE12345/icon0.png
	if (wildcard_match(req->resource, "/PSV/\?\?\?\?\?\?\?\?\?/icon0.png"))
	{
		asprintf(&res->data, PSV_ICONS_PATH_HDD, req->resource + 5);
		return (file_exists(res->data) == SUCCESS);
	}

	return 0;
}

static void enableWebServer(dWebReqHandler_t handler, void* data, int port)
{
	SceNetCtlInfo ip_info;

	memset(&ip_info, 0, sizeof(SceNetCtlInfo));
	sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &ip_info);
	LOG("Starting local web server %s:%d ...", ip_info.ip_address, port);

	if (dbg_webserver_start(port, handler, data))
	{
		show_message("%s http://%s:%d\n%s", _("Web Server on:"), ip_info.ip_address, port, _("Press OK to stop the Server."));
		dbg_webserver_stop();
	}
	else show_message(_("Error starting Web Server!"));
}

static void copyAllSavesUSB(const save_entry_t* save, int dev, int all)
{
	char dst_path[256];
	char copy_path[256];
	char save_path[256];
	int done = 0, err_count = 0;
	uint64_t progress = 0;
	list_node_t *node;
	save_entry_t *item;
	list_t *list = ((void**)save->dir_name)[0];

	_set_dest_path(dst_path, dev, PSV_SAVES_PATH_USB);
	if (!list || mkdirs(dst_path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Folder is not available:"), dst_path);
		return;
	}

	init_progress_bar("Copying all saves...");

	LOG("Copying all saves to '%s'...", dst_path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (!all && !(item->flags & SAVE_FLAG_SELECTED))
			continue;

		snprintf(copy_path, sizeof(copy_path), "%s%s_%s/", dst_path, item->title_id, item->dir_name);
		LOG("Copying <%s> to %s...", item->path, copy_path);

		if (item->type == FILE_TYPE_PSV && vita_SaveMount(item))
		{
			(copy_directory(item->path, item->path, copy_path) == SUCCESS) ? done++ : err_count++;
			vita_SaveUmount();
		}

		if (item->type == FILE_TYPE_PSP)
			(copy_directory(item->path, item->path, copy_path) == SUCCESS) ? done++ : err_count++;
	}

	end_progress_bar();
	show_message("%d/%d %s\n%s", done, done+err_count, _("Saves copied to:"), dst_path);
}

static int _copy_trophyset(const save_entry_t* trop, const char* out_path)
{
	char src_path[256];
	char exp_path[256];

	snprintf(exp_path, sizeof(exp_path), "%s%s/data/", out_path, trop->title_id);
	if (mkdirs(exp_path) != SUCCESS)
		return 0;

	LOG("Copying <%s> to %s...", trop->path, exp_path);
	copy_directory(trop->path, trop->path, exp_path);

	snprintf(src_path, sizeof(src_path), TROPHY_PATH_HDD "conf/%s/", apollo_config.user_id, trop->title_id);
	snprintf(exp_path, sizeof(exp_path), "%s%s/conf/", out_path, trop->title_id);
	mkdirs(exp_path);
	LOG("Copying <%s> to %s...", src_path, exp_path);
	copy_directory(src_path, src_path, exp_path);

	src_path[1] = 'x';
	snprintf(exp_path, sizeof(exp_path), "%s%s/conf_ux0/", out_path, trop->title_id);
	mkdirs(exp_path);
	LOG("Copying <%s> to %s...", src_path, exp_path);
	copy_directory(src_path, src_path, exp_path);

	return 1;
}

static void copyTrophy(const save_entry_t* trop, int dev)
{
	int ret;
	char exp_path[256];

	_set_dest_path(exp_path, dev, TROPHIES_PATH_USB);
	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Export folder is not available:"), exp_path);
		return;
	}

	init_loading_screen("Copying trophy...");
	ret = _copy_trophyset(trop, exp_path);
	stop_loading_screen();

	if (ret)
		show_message("%s\n%s%s", _("Trophy Set successfully copied to:"), exp_path, trop->title_id);
	else
		show_message(_("Error! Trophy Set %s not copied!"), trop->title_id);
}

static void copyAllTrophies(const save_entry_t* save, int dev, int all)
{
	char out_path[256];
	int done = 0, err_count = 0;
	list_node_t *node;
	save_entry_t *item;
	uint64_t progress = 0;
	list_t *list = ((void**)save->dir_name)[0];

	_set_dest_path(out_path, dev, TROPHIES_PATH_USB);
	if (!list || mkdirs(out_path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Export folder is not available:"), out_path);
		return;
	}

	init_progress_bar("Copying trophies...");

	LOG("Copying all trophies from '%s'...", save->path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (item->type != FILE_TYPE_TRP || !(all || item->flags & SAVE_FLAG_SELECTED) || !vita_SaveMount(item))
			continue;

		(_copy_trophyset(item, out_path) ? done++ : err_count++);
		vita_SaveUmount();
	}

	end_progress_bar();

	show_message("%d/%d %s\n%s", done, done+err_count, _("Trophy Sets copied to:"), out_path);
}

void exportLicenseZRif(const char* fname, const char* exp_path)
{
	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Export folder is not available:"), exp_path);
		return;
	}

	LOG("Exporting zRIF from '%s'...", fname);

	if (make_key_zrif(fname, exp_path))
		show_message("%s\n%s", _("zRIF successfully exported to:"), exp_path);
	else
		show_message(_("Error! zRIF not exported!"));
}

static int apply_sfo_patches(save_entry_t* entry, sfo_patch_t* patch)
{
    option_value_t* optval;
    code_entry_t* code;
    char in_file_path[256];
    char tmp_dir[SFO_DIRECTORY_SIZE];
    u8 tmp_psid[SFO_PSID_SIZE];
    list_node_t* node;

    if (entry->flags & SAVE_FLAG_PSP)
        return 1;

    for (node = list_head(entry->codes); (code = list_get(node)); node = list_next(node))
    {
        if (!code->activated || code->type != PATCH_SFO)
            continue;

        LOG("Active: [%s]", code->name);

        switch (code->codes[0])
        {
        case SFO_CHANGE_ACCOUNT_ID:
            if (entry->flags & SAVE_FLAG_OWNER)
                entry->flags ^= SAVE_FLAG_OWNER;

            optval = list_get_item(code->options[0].opts, code->options[0].sel);
            sscanf(optval->value, "%" PRIx64, &patch->account_id);
            break;

        case SFO_REMOVE_PSID:
            memset(tmp_psid, 0, SFO_PSID_SIZE);
            patch->psid = tmp_psid;
            break;

        case SFO_CHANGE_TITLE_ID:
            patch->directory = strstr(entry->path, entry->title_id);
            snprintf(in_file_path, sizeof(in_file_path), "%s", entry->path);
            strncpy(tmp_dir, patch->directory, SFO_DIRECTORY_SIZE);

            optval = list_get_item(code->options[0].opts, code->options[0].sel);
            strncpy(entry->title_id, optval->name, 9);
            strncpy(patch->directory, entry->title_id, 9);
            strncpy(tmp_dir, entry->title_id, 9);
            *strrchr(tmp_dir, '/') = 0;
            patch->directory = tmp_dir;

            LOG("Moving (%s) -> (%s)", in_file_path, entry->path);
            rename(in_file_path, entry->path);
            break;

        default:
            break;
        }

        code->activated = 0;
    }

	snprintf(in_file_path, sizeof(in_file_path), "%s" "sce_sys/param.sfo", selected_entry->path);
	LOG("Applying SFO patches '%s'...", in_file_path);

	return (patch_sfo(in_file_path, patch) == SUCCESS);
}

static int psp_is_decrypted(list_t* list, const char* fname)
{
	list_node_t *node;

	for (node = list_head(list); node; node = list_next(node))
		if (strcmp(list_get(node), fname) == 0)
			return 1;

	return 0;
}

static void* vita_host_callback(int id, uint32_t* size)
{
	memset(host_buf, 0, sizeof(host_buf));

	switch (id)
	{
	case APOLLO_HOST_TEMP_PATH:
		if (size) *size = strlen(APOLLO_LOCAL_CACHE);
		return APOLLO_LOCAL_CACHE;

	case APOLLO_HOST_DATA_PATH:
		if (size) *size = strlen(APOLLO_DATA_PATH);
		return APOLLO_DATA_PATH;

	case APOLLO_HOST_SYS_NAME:
		if (size) *size = 11;
		return "Apollo-Vita";

	case APOLLO_HOST_PSID:
		memcpy(host_buf, apollo_config.psid, 16);
		if (size) *size = 16;
		return host_buf;

	case APOLLO_HOST_ACCOUNT_ID:
		memcpy(host_buf, &apollo_config.account_id, 8);
		*(uint64_t*)host_buf = ES64(*(uint64_t*)host_buf);
		if (size) *size = 8;
		return host_buf;

	case APOLLO_HOST_USERNAME:
		strncpy(host_buf, menu_about_strings_project[1], sizeof(host_buf));
		if (size) *size = strlen(host_buf);
		return host_buf;

	case APOLLO_HOST_LAN_ADDR:
	case APOLLO_HOST_WLAN_ADDR:
		if (sceNetGetMacAddress((SceNetEtherAddr*) host_buf, 0) < 0)
			LOG("Error getting Wlan Ethernet Address");

		if (size) *size = 6;
		return host_buf;
	}

	if (size) *size = 1;
	return host_buf;
}

static int apply_cheat_patches(const save_entry_t* entry)
{
	int ret = 1;
	char tmpfile[256];
	char* filename;
	code_entry_t* code;
	list_node_t* node;
	list_t* decrypted_files = list_alloc();
	uint8_t key[16];

	init_loading_screen("Applying changes...");

	for (node = list_head(entry->codes); (code = list_get(node)); node = list_next(node))
	{
		if (!code->activated || (code->type != PATCH_GAMEGENIE && code->type != PATCH_BSD && code->type != PATCH_PYTHON))
			continue;

		LOG("Active code: [%s]", code->name);

		if (strrchr(code->file, '\\'))
			filename = strrchr(code->file, '\\')+1;
		else
			filename = code->file;

		if (strchr(filename, '*'))
		{
			option_value_t* optval = list_get_item(code->options[0].opts, code->options[0].sel);
			filename = optval->name;
		}
		
		if (strncmp(code->file, "~extracted\\", 11) == 0)
			snprintf(tmpfile, sizeof(tmpfile), "%s", code->file);
		else
		{
			snprintf(tmpfile, sizeof(tmpfile), "%s%s", entry->path, filename);

			if (entry->flags & SAVE_FLAG_PSP && !psp_is_decrypted(decrypted_files, filename))
			{
				if (get_psp_save_key(entry, key) && psp_DecryptSavedata(entry->path, tmpfile, key))
				{
					LOG("Decrypted PSP file '%s'", filename);
					list_append(decrypted_files, strdup(filename));
				}
				else
				{
					LOG("Error: failed to decrypt (%s)", filename);
					ret = 0;
					continue;
				}
			}
		}

		if (!apply_cheat_patch_code(tmpfile, code, &vita_host_callback))
		{
			LOG("Error: failed to apply (%s)", code->name);
			ret = 0;
		}

		code->activated = 0;
	}

	for (node = list_head(decrypted_files); (filename = list_get(node)); node = list_next(node))
	{
		LOG("Encrypting '%s'...", filename);
		if (!get_psp_save_key(entry, key) || !psp_EncryptSavedata(entry->path, filename, key))
		{
			LOG("Error: failed to encrypt (%s)", filename);
			ret = 0;
		}

		free(filename);
	}

	list_free(decrypted_files);
	free_patch_var_list();
	stop_loading_screen();

	return ret;
}

static void resignSave(save_entry_t* entry)
{
    sfo_patch_t patch = {
        .flags = 0,
        .user_id = apollo_config.user_id,
        .directory = NULL,
        .account_id = apollo_config.account_id,
    };

    LOG("Resigning save '%s'...", entry->name);

    if (!apply_sfo_patches(entry, &patch))
        show_message(_("Error! Account changes couldn't be applied"));

    LOG("Applying cheats to '%s'...", entry->name);
    if (!apply_cheat_patches(entry))
        show_message(_("Error! Cheat codes couldn't be applied"));

    if (entry->type == FILE_TYPE_PSP && !psp_ResignSavedata(entry->path))
        show_message(_("Error! PSP Save couldn't be resigned"));

    show_message(_("Save %s successfully modified!"), entry->title_id);
}

static void resignAllSaves(const save_entry_t* save, int all)
{
	char sfoPath[256];
	int err_count = 0, done = 0;
	list_node_t *node;
	save_entry_t *item;
	uint64_t progress = 0;
	list_t *list = ((void**)save->dir_name)[0];
	sfo_patch_t patch = {
		.user_id = apollo_config.user_id,
//		.psid = (u8*) apollo_config.psid,
		.account_id = apollo_config.account_id,
	};

	init_progress_bar("Resigning all saves...");

	LOG("Resigning all saves from '%s'...", save->path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if ((item->flags & SAVE_FLAG_LOCKED) || !(all || item->flags & SAVE_FLAG_SELECTED))
			continue;

		if (item->type == FILE_TYPE_PSP)
		{
			psp_ResignSavedata(item->path) ? done++ : err_count++;
			continue;
		}

		if (item->type != FILE_TYPE_PSV)
			continue;

		snprintf(sfoPath, sizeof(sfoPath), "%s" "sce_sys/param.sfo", item->path);
		if (file_exists(sfoPath) != SUCCESS)
			continue;

		LOG("Patching SFO '%s'...", sfoPath);
		(patch_sfo(sfoPath, &patch) == SUCCESS) ? done++ : err_count++;
	}

	end_progress_bar();

	show_message("%d/%d %s", done, done+err_count, _("Saves successfully resigned"));
}

static char* get_title_name_icon(const save_entry_t* item)
{
	char *ret = NULL;
	char iconfile[256];
	char local_file[256];

	LOG("Getting data for '%s'...", item->title_id);

	if (get_name_title_id(item->title_id, local_file))
		ret = strdup(local_file);
	else
		ret = strdup(item->name);

	LOG("Get Vita icon %s (%s)", item->title_id, ret);
	snprintf(local_file, sizeof(local_file), APOLLO_LOCAL_CACHE "%.9s.PNG", item->title_id);
	if (file_exists(local_file) == SUCCESS)
		return ret;

	snprintf(iconfile, sizeof(iconfile), PSV_ICONS_PATH_HDD "/icon0.png", item->title_id);
	copy_file(iconfile, local_file);

	return ret;
}

static char* get_title_icon_psx(const save_entry_t* entry)
{
	FILE* fp;
	uint8_t* icon = NULL;
	char *ret = NULL;
	char path[256];
	char type[4] = {'-', '1', 'p', 'v'};

	LOG("Getting data for '%s'...", entry->title_id);
	snprintf(path, sizeof(path), APOLLO_DATA_PATH "ps%ctitleid.txt", type[entry->type]);
	fp = fopen(path, "r");
	if (fp)
	{
		while(!ret && fgets(path, sizeof(path), fp))
		{
			if (strncmp(path, entry->title_id, 9) != 0)
				continue;

			path[strlen(path)-1] = 0;
			ret = strdup(path+10);
		}
		fclose(fp);
	}

	if (!ret)
		ret = strdup(entry->name);

	LOG("Get ps%c icon %s (%s)", type[entry->type], entry->title_id, ret);
	snprintf(path, sizeof(path), APOLLO_LOCAL_CACHE "%.9s.PNG", entry->title_id);
	if (file_exists(path) == SUCCESS)
		return ret;

	fp = fopen(path, "wb");
	if (entry->type == FILE_TYPE_PS1)
	{
		icon = getIconRGBA(entry->blocks, 0);
		svpng(fp, 16, 16, icon, 1);
	}
	else
	{
		//PSP
		size_t sz;
		snprintf(path, sizeof(path), "%sICON0.PNG", entry->path);
		if (read_buffer(path, &icon, &sz) == SUCCESS)
			fwrite(icon, sz, 1, fp);
	}
	free(icon);
	fclose(fp);

	return ret;
}

static void get_psv_filename(char* psvName, const char* path, const char* dirName)
{
	char tmpName[4];

	sprintf(psvName, "%s%.12s", path, dirName);
	for (const char *ch = &dirName[12]; *ch; *ch++)
	{
		snprintf(tmpName, sizeof(tmpName), "%02X", *ch);
		strcat(psvName, tmpName);
	}
	strcat(psvName, ".PSV");
}

static void uploadSaveFTP(const save_entry_t* save)
{
	FILE* fp;
	char *tmp;
	char remote[256];
	char local[256];
	int ret = 0;
	struct tm t;
	char type[4] = {'-', '1', 'P', 'V'};

	if (!show_dialog(DIALOG_TYPE_YESNO, _("Do you want to upload %s?"), save->dir_name))
		return;

	init_loading_screen("Sync with FTP Server...");

	snprintf(remote, sizeof(remote), "%s%016" PRIX64 "/PS%c/", apollo_config.ftp_url, apollo_config.account_id, type[save->type]);
	http_download(remote, "games.txt", APOLLO_LOCAL_CACHE "games.ftp", 0);

	snprintf(remote, sizeof(remote), "%s%016" PRIX64 "/PS%c/%s/", apollo_config.ftp_url, apollo_config.account_id, type[save->type], save->title_id);
	http_download(remote, "saves.txt", APOLLO_LOCAL_CACHE "saves.ftp", 0);
	http_download(remote, "checksum.sfv", APOLLO_LOCAL_CACHE "sfv.ftp", 0);

	gmtime_r(&(time_t){time(NULL)}, &t);
	snprintf(local, sizeof(local), APOLLO_LOCAL_CACHE "%s_%d-%02d-%02d-%02d%02d%02d.zip",
			(save->type == FILE_TYPE_PS1) ? save->title_id : save->dir_name,
			t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

	if (save->type != FILE_TYPE_PS1)
	{
		tmp = strdup(save->path);
		*strrchr(tmp, '/') = 0;
		*strrchr(tmp, '/') = 0;
	
		ret = zip_directory(tmp, save->path, local);
		free(tmp);
	}
	else
	{
		tmp = malloc(256);

		ret = saveSingleSave(APOLLO_LOCAL_CACHE, save->blocks, PS1SAVE_PSV);
		get_psv_filename(tmp, APOLLO_LOCAL_CACHE, save->dir_name);
		ret &= zip_file(tmp, local);

		unlink_secure(tmp);
		free(tmp);
	}

	stop_loading_screen();
	if (!ret)
	{
		show_message("%s\n%s", _("Error! Couldn't zip save:"), save->dir_name);
		return;
	}

	tmp = strrchr(local, '/')+1;
	uint32_t crc = file_crc32(local);

	LOG("Updating %s save index...", save->title_id);
	fp = fopen(APOLLO_LOCAL_CACHE "saves.ftp", "a");
	if (fp)
	{
		fprintf(fp, "%s=[%s] %d-%02d-%02d %02d:%02d:%02d %s (CRC: %08X)\r\n", tmp, save->dir_name,
				t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, save->name, crc);
		fclose(fp);
	}

	LOG("Updating .sfv CRC32: %08X", crc);
	fp = fopen(APOLLO_LOCAL_CACHE "sfv.ftp", "a");
	if (fp)
	{
		fprintf(fp, "%s %08X\n", tmp, crc);
		fclose(fp);
	}

	ret = ftp_upload(local, remote, tmp, 1);
	ret &= ftp_upload(APOLLO_LOCAL_CACHE "saves.ftp", remote, "saves.txt", 1);
	ret &= ftp_upload(APOLLO_LOCAL_CACHE "sfv.ftp", remote, "checksum.sfv", 1);

	unlink_secure(local);
	tmp = readTextFile(APOLLO_LOCAL_CACHE "games.ftp");
	if (!tmp)
		tmp = strdup("");

	if (strstr(tmp, save->title_id) == NULL)
	{
		LOG("Updating games index...");
		free(tmp);
		tmp = (save->type == FILE_TYPE_PSV) ? get_title_name_icon(save) : get_title_icon_psx(save);

		snprintf(local, sizeof(local), APOLLO_LOCAL_CACHE "%.9s.PNG", save->title_id);
		ret &= ftp_upload(local, remote, (save->type == FILE_TYPE_PSP) ? "ICON0.PNG" : "icon0.png", 1);

		fp = fopen(APOLLO_LOCAL_CACHE "games.ftp", "a");
		if (fp)
		{
			fprintf(fp, "%s=%s\r\n", save->title_id, tmp);
			fclose(fp);
		}

		snprintf(remote, sizeof(remote), "%s%016" PRIX64 "/PS%c/", apollo_config.ftp_url, apollo_config.account_id, type[save->type]);
		ret &= ftp_upload(APOLLO_LOCAL_CACHE "games.ftp", remote, "games.txt", 1);
	}
	free(tmp);
	clean_directory(APOLLO_LOCAL_CACHE, ".ftp");

	if (ret)
		show_message("%s\n%s", _("Save successfully uploaded:"), save->dir_name);
	else
		show_message("%s\n%s", _("Error! Couldn't upload save:"), save->dir_name);
}

static void import_mcr2vmp(const save_entry_t* save, const char* src)
{
	char mcrPath[256];
	uint8_t *data = NULL;
	size_t size = 0;

	snprintf(mcrPath, sizeof(mcrPath), PS1_SAVES_PATH_HDD "%s/%s", save->title_id, src);
	read_buffer(mcrPath, &data, &size);

	if (openMemoryCardStream(data, size, 0))
		show_message("%s\n%s", _("Memory card successfully imported to:"), save->path);
	else
		show_message("%s\n%s", _("Error importing memory card:"), mcrPath);
}

static void export_vmp2mcr(const save_entry_t* save)
{
	char mcrPath[256];

	snprintf(mcrPath, sizeof(mcrPath), PS1_SAVES_PATH_HDD "%s/%s", save->title_id, strrchr(save->path, '/') + 1);
	strcpy(strrchr(mcrPath, '.'), ".MCR");
	mkdirs(mcrPath);

	if (saveMemoryCard(mcrPath, PS1CARD_RAW, 0))
		show_message("%s\n%s", _("Memory card successfully exported to:"), mcrPath);
	else
		show_message("%s\n%s", _("Error exporting memory card:"), save->path);
}

static int _copy_save_file(const char* src_path, const char* dst_path, const char* filename)
{
	char src[256], dst[256];

	snprintf(src, sizeof(src), "%s%s", src_path, filename);
	snprintf(dst, sizeof(dst), "%s%s", dst_path, filename);

	return (copy_file(src, dst) == SUCCESS);
}

static void decryptSaveFile(const save_entry_t* entry, const char* filename)
{
	char path[256];
	uint8_t key[16];

	if (entry->flags & SAVE_FLAG_PSP && !get_psp_save_key(entry, key))
	{
		show_message(_("Error! No game decryption key available for %s"), entry->title_id);
		return;
	}

	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s/", apollo_config.user_id, entry->dir_name);
	mkdirs(path);

	LOG("Decrypt '%s%s' to '%s'...", entry->path, filename, path);

	if (_copy_save_file(entry->path, path, filename))
	{
		strlcat(path, filename, sizeof(path));
		if (entry->flags & SAVE_FLAG_PSP && !psp_DecryptSavedata(entry->path, path, key))
			show_message(_("Error! File %s couldn't be exported"), filename);

		show_message("%s\n%s", _("File successfully exported to:"), path);
	}
	else
		show_message(_("Error! File %s couldn't be exported"), filename);
}

static void encryptSaveFile(const save_entry_t* entry, const char* filename)
{
	char path[256];
	uint8_t key[16];

	if (entry->flags & SAVE_FLAG_PSP && !get_psp_save_key(entry, key))
	{
		show_message(_("Error! No game decryption key available for %s"), entry->title_id);
		return;
	}

	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s/%s", apollo_config.user_id, entry->dir_name, filename);
	if (file_exists(path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Can't find decrypted save-game file:"), path);
		return;
	}

	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s/", apollo_config.user_id, entry->dir_name);
	LOG("Encrypt '%s%s' to '%s'...", path, filename, entry->path);

	if (_copy_save_file(path, entry->path, filename))
	{
		if (entry->flags & SAVE_FLAG_PSP && !psp_EncryptSavedata(entry->path, filename, key))
			show_message(_("Error! File %s couldn't be imported"), filename);

		show_message("%s\n%s%s", _("File successfully imported to:"), entry->path, filename);
	}
	else
		show_message(_("Error! File %s couldn't be imported"), filename);
}

static void downloadLink(const char* path)
{
	char url[256] = "http://";
	char out_path[256];

	if (!osk_dialog_get_text("Download URL", url, sizeof(url)))
		return;

	char *fname = strrchr(url, '/');
	snprintf(out_path, sizeof(out_path), "%s%s", path, fname ? ++fname : "download.bin");

	if (http_download(url, NULL, out_path, 1))
		show_message("%s\n%s", _("File successfully downloaded to:"), out_path);
	else
		show_message(_("Error! File couldn't be downloaded"));
}

void execCodeCommand(code_entry_t* code, const char* codecmd)
{
	option_value_t* optval;

	if (selected_entry->flags & SAVE_FLAG_PSV && selected_entry->flags & SAVE_FLAG_HDD && !vita_SaveMount(selected_entry))
	{
		LOG("Error Mounting Save! Check Save Mount Patches");
		return;
	}

	switch (codecmd[0])
	{
		case CMD_DECRYPT_FILE:
			optval = list_get_item(code->options[0].opts, code->options[0].sel);
			decryptSaveFile(selected_entry, optval->name);
			code->activated = 0;
			break;

		case CMD_DOWNLOAD_USB:
			downloadSave(selected_entry, code->file, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_DOWNLOAD_HDD:
			downloadSaveHDD(selected_entry, code->file);
			code->activated = 0;
			break;

		case CMD_UPLOAD_SAVE:
			uploadSaveFTP(selected_entry);
			code->activated = 0;
			break;

		case CMD_EXPORT_ZIP_USB:
			zipSave(selected_entry, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_COPY_SAVE_USB:
			copySave(selected_entry, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_COPY_SAVE_HDD:
			copySaveHDD(selected_entry);
			code->activated = 0;
			break;

		case CMD_EXP_KEYSTONE:
		case CMD_IMP_KEYSTONE:
			copyKeystone(selected_entry, codecmd[0] == CMD_IMP_KEYSTONE);
			code->activated = 0;
			break;

		case CMD_EXP_PSPKEY:
			pspExportKey(selected_entry);
			code->activated = 0;
			break;

		case CMD_DUMP_PSPKEY:
			pspDumpKey(selected_entry);
			code->activated = 0;
			break;

		case CMD_SETUP_PLUGIN:
			show_message(install_sgkey_plugin(codecmd[1]) ? "Plugin successfully %s" : "Error! Plugin couldn't be %s", codecmd[1] ? "installed" : "disabled");
			code->activated = 0;
			break;

		case CMD_SETUP_FUSEDUMP:
			show_message(install_fuseid_dumper() ? "PSP FuseID app successfully installed" : "Error! PSP app couldn't be installed");
			code->activated = 0;
			break;

		case CMD_IMP_MCR2VMP:
			optval = list_get_item(code->options[0].opts, code->options[0].sel);
			import_mcr2vmp(selected_entry, optval->name);
			selected_entry->flags |= SAVE_FLAG_UPDATED;
			code->activated = 0;
			break;

		case CMD_EXP_VMP2MCR:
			export_vmp2mcr(selected_entry);
			code->activated = 0;
			break;

		case CMD_RESIGN_VMP:
			if (vmp_resign(selected_entry->path))
				show_message("%s\n%s", _("Memory card successfully resigned:"), selected_entry->path);
			else
				show_message("%s\n%s", _("Error resigning memory card:"), selected_entry->path);
			code->activated = 0;
			break;

		case CMD_EXP_TROPHY_USB:
			copyTrophy(selected_entry, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_IMP_TROPHY_HDD:
			importTrophy(selected_entry->path, code->file);
			code->activated = 0;
			break;

		case CMD_ZIP_TROPHY_USB:
			exportTrophyZip(selected_entry, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_COPY_TROPHIES_USB:
		case CMD_COPY_ALL_TROP_USB:
			copyAllTrophies(selected_entry, codecmd[1], codecmd[0] == CMD_COPY_ALL_TROP_USB);
			code->activated = 0;
			break;

		case CMD_COPY_SAVES_USB:
		case CMD_COPY_ALL_SAVES_USB:
			copyAllSavesUSB(selected_entry, codecmd[1], codecmd[0] == CMD_COPY_ALL_SAVES_USB);
			code->activated = 0;
			break;

		case CMD_EXP_SAVES_VMC:
		case CMD_EXP_ALL_SAVES_VMC:
			exportAllSavesVMC(selected_entry, codecmd[1], codecmd[0] == CMD_EXP_ALL_SAVES_VMC);
			code->activated = 0;
			break;

		case CMD_EXP_VMCSAVE:
			exportVmcSave(selected_entry, code->options[0].id, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_IMP_VMCSAVE:
			if (openSingleSave(code->file, (int*) host_buf))
				show_message("%s\n%s", _("Save successfully imported:"), code->file);
			else
				show_message("%s\n%s", _("Error! Couldn't import save:"), code->file);

			selected_entry->flags |= SAVE_FLAG_UPDATED;
			code->activated = 0;
			break;

		case CMD_DELETE_SAVE:
			if (deleteSave(selected_entry))
				selected_entry->flags |= SAVE_FLAG_UPDATED;
			else
				code->activated = 0;
			break;

		case CMD_EXP_FINGERPRINT:
			exportFingerprint(selected_entry, 0);
			code->activated = 0;
			break;

		case CMD_DUMP_FINGERPRINTS:
			dumpAllFingerprints(selected_entry);
			code->activated = 0;
			break;

		case CMD_RESIGN_SAVE:
			resignSave(selected_entry);
			code->activated = 0;
			break;

		case CMD_RESIGN_SAVES:
		case CMD_RESIGN_ALL_SAVES:
			resignAllSaves(selected_entry, codecmd[0] == CMD_RESIGN_ALL_SAVES);
			code->activated = 0;
			break;

		case CMD_COPY_SAVES_HDD:
		case CMD_COPY_ALL_SAVES_HDD:
			copyAllSavesHDD(selected_entry, codecmd[0] == CMD_COPY_ALL_SAVES_HDD);
			code->activated = 0;
			break;

		case CMD_SAVE_WEBSERVER:
			enableWebServer(webReqHandler, ((void**)selected_entry->dir_name)[0], 8080);
			code->activated = 0;
			break;

		case CMD_IMPORT_DATA_FILE:
			optval = list_get_item(code->options[0].opts, code->options[0].sel);
			encryptSaveFile(selected_entry, optval->name);
			code->activated = 0;
			break;

		case CMD_EXP_LIC_ZRIF:
			exportLicenseZRif(code->file, APOLLO_PATH "zrif/");
			code->activated = 0;
			break;

		case CMD_EXTRACT_ARCHIVE:
			extractArchive(code->file);
			code->activated = 0;
			break;

		case CMD_URL_DOWNLOAD:
			downloadLink(selected_entry->path);
			code->activated = 0;
			break;

		case CMD_NET_WEBSERVER:
			enableWebServer(dbg_simpleWebServerHandler, NULL, 8080);
			code->activated = 0;
			break;

		case CMD_CONV_CSO2ISO:
			if (convert_cso2iso(code->file))
				show_message("%s\n%s", _("ISO successfully saved to:"), selected_entry->path);
			else
				show_message(_("Error! ISO couldn't be created"));
			code->activated = 0;
			break;

		case CMD_CONV_ISO2CSO:
			if (convert_iso2cso(code->file))
				show_message("%s\n%s", _("CSO successfully saved to:"), selected_entry->path);
			else
				show_message(_("Error! CSO couldn't be created"));
			code->activated = 0;
			break;

		default:
			break;
	}

	if (selected_entry->flags & SAVE_FLAG_PSV && selected_entry->flags & SAVE_FLAG_HDD)
		vita_SaveUmount();

	return;
}
