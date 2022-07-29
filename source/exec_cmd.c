#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <polarssl/md5.h>

#include "saves.h"
#include "menu.h"
#include "common.h"
#include "utils.h"
#include "sfo.h"


static int _set_dest_path(int dest, char* path)
{
	switch (dest)
	{
	case STORAGE_UMA0:
		strcpy(path, SAVES_PATH_UMA0);
		break;

	case STORAGE_IMC0:
		strcpy(path, SAVES_PATH_IMC0);
		break;

	case STORAGE_UX0:
		strcpy(path, UX0_PATH PSV_SAVES_PATH_USB);
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

	_set_dest_path(dst, path);
	if (mkdirs(path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", path);
		return;
	}

	if (!http_download(entry->path, file, APOLLO_LOCAL_CACHE "tmpsave.zip", 1))
	{
		show_message("Error downloading save game from:\n%s%s", entry->path, file);
		return;
	}

	if (extract_zip(APOLLO_LOCAL_CACHE "tmpsave.zip", path))
		show_message("Save game successfully downloaded to:\n%s", path);
	else
		show_message("Error extracting save game!");

	unlink_secure(APOLLO_LOCAL_CACHE "tmpsave.zip");
}

static uint32_t get_filename_id(const char* dir)
{
	char path[128];
	uint32_t tid = 0;

	do
	{
		tid++;
		snprintf(path, sizeof(path), "%s%08d.zip", dir, tid);
	}
	while (file_exists(path) == SUCCESS);

	return tid;
}

static void zipSave(const char* exp_path)
{
	char* export_file;
	char* tmp;
	uint32_t fid;

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", exp_path);
		return;
	}

	init_loading_screen("Exporting save game...");

	fid = get_filename_id(exp_path);
	asprintf(&export_file, "%s%08d.zip", exp_path, fid);

	tmp = strdup(selected_entry->path);
	*strrchr(tmp, '/') = 0;
	*strrchr(tmp, '/') = 0;

	zip_directory(tmp, selected_entry->path, export_file);

	sprintf(export_file, "%s%08x.txt", exp_path, apollo_config.user_id);
	FILE* f = fopen(export_file, "a");
	if (f)
	{
		fprintf(f, "%08d.zip=[%s] %s\n", fid, selected_entry->title_id, selected_entry->name);
		fclose(f);
	}

	sprintf(export_file, "%s%08x.xml", exp_path, apollo_config.user_id);
//	save_xml_owner(export_file);

	free(export_file);
	free(tmp);

	stop_loading_screen();
	show_message("Zip file successfully saved to:\n%s%08d.zip", exp_path, fid);
}

static void copySave(const save_entry_t* save, const char* exp_path)
{
	char* copy_path;

	if (strncmp(save->path, exp_path, strlen(exp_path)) == 0)
	{
		show_message("Copy operation cancelled!\nSame source and destination.");
		return;
	}

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", exp_path);
		return;
	}

	init_loading_screen("Copying files...");

	asprintf(&copy_path, "%s%08x_%s_%s/", exp_path, apollo_config.user_id, save->title_id, save->dir_name);

	LOG("Copying <%s> to %s...", save->path, copy_path);
	copy_directory(save->path, save->path, copy_path);

	free(copy_path);

	stop_loading_screen();
	show_message("Files successfully copied to:\n%s", exp_path);
}

static int get_psp_save_key(const save_entry_t* entry, uint8_t* key)
{
	char path[256];

	snprintf(path, sizeof(path), "ux0:pspemu/PSP/SAVEPLAIN/%s/%s.bin", entry->dir_name, entry->title_id);
	return (read_psp_game_key(path, key));
}

static int _copy_save_hdd(const save_entry_t* save)
{
	char copy_path[256];
	char mount[32];

	if (!vita_SaveMount(save, mount))
		return 0;

	snprintf(copy_path, sizeof(copy_path), APOLLO_SANDBOX_PATH, save->dir_name);

	LOG("Copying <%s> to %s...", save->path, copy_path);
	copy_directory(save->path, save->path, copy_path);

	vita_SaveUmount(mount);
	return 1;
}

static int _copy_save_psp(const save_entry_t* save)
{
	char copy_path[256];

	snprintf(copy_path, sizeof(copy_path), PSP_SAVES_PATH_HDD "%s/", save->dir_name);

	LOG("Copying <%s> to %s...", save->path, copy_path);
	return (copy_directory(save->path, save->path, copy_path) == SUCCESS);
}

static void copySaveHDD(const save_entry_t* save)
{
	//source save is already on HDD
	if (save->flags & SAVE_FLAG_HDD)
	{
		show_message("Copy operation cancelled!\nSame source and destination.");
		return;
	}

	init_loading_screen("Copying save game...");
	int ret = (save->flags & SAVE_FLAG_PSP) ? _copy_save_psp(save) : _copy_save_hdd(save);
	stop_loading_screen();

	if (ret)
		show_message("Files successfully copied to:\n%s/%s", save->title_id, save->dir_name);
	else
		show_message("Error! Can't copy Save-game folder:\n%s/%s", save->title_id, save->dir_name);
}

static void copyAllSavesHDD(const save_entry_t* save, int all)
{
	int err_count = 0;
	list_node_t *node;
	save_entry_t *item;
	uint64_t progress = 0;
	list_t *list = ((void**)save->dir_name)[0];

	init_progress_bar("Copying all saves...");

	LOG("Copying all saves from '%s' to HDD...", save->path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (item->type == FILE_TYPE_PSV && !(item->flags & SAVE_FLAG_LOCKED) && (all || item->flags & SAVE_FLAG_SELECTED))
			err_count += ! _copy_save_hdd(item);
	}

	end_progress_bar();

	if (err_count)
		show_message("Error: %d Saves couldn't be copied to Internal Storage", err_count);
	else
		show_message("All Saves copied to Internal Storage");
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
		show_message("All files extracted to:\n%s", exp_path);
	else
		show_message("Error: %s couldn't be extracted", file_path);
}

static void exportFingerprint(const save_entry_t* save, int silent)
{
	char fpath[256];
	uint8_t buffer[0x40];

	snprintf(fpath, sizeof(fpath), "%ssce_sys/keystone", save->path);
	LOG("Reading '%s' ...", fpath);

	if (read_file(fpath, buffer, sizeof(buffer)) != SUCCESS)
	{
		if (!silent) show_message("Error! Keystone file is not available:\n%s", fpath);
		return;
	}

	snprintf(fpath, sizeof(fpath), APOLLO_PATH "fingerprints.txt");
	FILE *fp = fopen(fpath, "a");
	if (!fp)
	{
		if (!silent) show_message("Error! Can't open file:\n%s", fpath);
		return;
	}

	fprintf(fp, "%s=", save->title_id);
	for (size_t i = 0x20; i < 0x40; i++)
		fprintf(fp, "%02x", buffer[i]);

	fprintf(fp, "\n");
	fclose(fp);

	if (!silent) show_message("%s fingerprint successfully saved to:\n%s", save->title_id, fpath);
}
/*
void exportTrophiesZip(const char* exp_path)
{
	char* export_file;
	char* trp_path;
	char* tmp;

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", exp_path);
		return;
	}

	init_loading_screen("Exporting Trophies ...");

	asprintf(&export_file, "%s" "trophies_%08d.zip", exp_path, apollo_config.user_id);
	asprintf(&trp_path, TROPHY_PATH_HDD, apollo_config.user_id);

	tmp = strdup(trp_path);
	*strrchr(tmp, '/') = 0;

	zip_directory(tmp, trp_path, export_file);

	sprintf(export_file, "%s" OWNER_XML_FILE, exp_path);
	_saveOwnerData(export_file);

	free(export_file);
	free(trp_path);
	free(tmp);

	stop_loading_screen();
	show_message("Trophies successfully saved to:\n%strophies_%08d.zip", exp_path, apollo_config.user_id);
}
*/
static void pspDumpKey(const save_entry_t* save)
{
	char fpath[256];
	uint8_t buffer[0x10];

	if (!get_psp_save_key(save, buffer))
	{
		show_message("Error! Game Key file is not available:\n%s/%s.bin", save->dir_name, save->title_id);
		return;
	}

	snprintf(fpath, sizeof(fpath), APOLLO_PATH "fingerprints.txt");
	FILE *fp = fopen(fpath, "a");
	if (!fp)
	{
		show_message("Error! Can't open file:\n%s", fpath);
		return;
	}

	fprintf(fp, "%s=", save->title_id);
	for (size_t i = 0; i < sizeof(buffer); i++)
		fprintf(fp, "%02x", buffer[i]);

	fprintf(fp, "\n");
	fclose(fp);

	show_message("%s fingerprint successfully saved to:\n%s", save->title_id, fpath);
}

static void pspExportKey(const save_entry_t* save)
{
	char fpath[256];
	uint8_t buffer[0x10];

	if (!get_psp_save_key(save, buffer))
	{
		show_message("Error! Game Key file is not available:\n%s/%s.bin", save->dir_name, save->title_id);
		return;
	}

	snprintf(fpath, sizeof(fpath), APOLLO_USER_PATH "%s/%s.bin", apollo_config.user_id, save->dir_name, save->title_id);
	mkdirs(fpath);

	if (write_buffer(fpath, buffer, sizeof(buffer)) == SUCCESS)
		show_message("%s game key successfully saved to:\n%s", save->title_id, fpath);
	else
		show_message("Error! Can't save file:\n%s", fpath);
}

static void dumpAllFingerprints(const save_entry_t* save)
{
	char mount[32];
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

		if (item->flags & SAVE_FLAG_PSV && item->flags & SAVE_FLAG_HDD)
			if (!vita_SaveMount(item, mount))
				continue;

		exportFingerprint(item, 1);

		if (item->flags & SAVE_FLAG_PSV && item->flags & SAVE_FLAG_HDD)
			vita_SaveUmount(mount);
	}

	end_progress_bar();
	show_message("All fingerprints dumped to:\n%sfingerprints.txt", APOLLO_PATH);
}

static void activateAccount(int user, const char* value)
{
	uint64_t account = 0;

	sscanf(value, "%lx", &account);
	if (!account)
		account = (0x6F6C6C6F70610000 + ((user & 0xFFFF) ^ 0xFFFF));

	LOG("Activating user=%d (%lx)...", user, account);
	if (1)
//	if (regMgr_SetAccountId(user, &account) != SUCCESS)
	{
		show_message("Error! Couldn't activate user account");
		return;
	}

	show_message("Account successfully activated!\nA system reboot might be required");
}

static void copySavePFS(const save_entry_t* save)
{
	char src_path[256];
	char hdd_path[256];
	char mount[32];
	sfo_patch_t patch = {
		.user_id = apollo_config.user_id,
		.account_id = apollo_config.account_id,
	};

	if (!vita_SaveMount(save, mount))
	{
		LOG("[!] Error: can't create/mount save!");
		return;
	}
	vita_SaveUmount(mount);

	snprintf(src_path, sizeof(src_path), "%s%s", save->path, save->dir_name);
	snprintf(hdd_path, sizeof(hdd_path), "/user/home/%08x/savedata/%s/sdimg_%s", apollo_config.user_id, save->title_id, save->dir_name);
	LOG("Copying <%s> to %s...", src_path, hdd_path);
	if (copy_file(src_path, hdd_path) != SUCCESS)
	{
		LOG("[!] Error: can't copy %s", hdd_path);
		return;
	}

	snprintf(src_path, sizeof(src_path), "%s%s.bin", save->path, save->dir_name);
	snprintf(hdd_path, sizeof(hdd_path), "/user/home/%08x/savedata/%s/%s.bin", apollo_config.user_id, save->title_id, save->dir_name);
	LOG("Copying <%s> to %s...", src_path, hdd_path);
	if (copy_file(src_path, hdd_path) != SUCCESS)
	{
		LOG("[!] Error: can't copy %s", hdd_path);
		return;
	}

	if (!vita_SaveMount(save, mount))
	{
		LOG("[!] Error: can't remount save");
		show_message("Error! Can't mount encrypted save.\n(incompatible save-game firmware version)");
		return;
	}

	snprintf(hdd_path, sizeof(hdd_path), APOLLO_SANDBOX_PATH "sce_sys/param.sfo", mount);
	if (show_dialog(1, "Resign save %s/%s?", save->title_id, save->dir_name))
		patch_sfo(hdd_path, &patch);

//	*strrchr(hdd_path, 'p') = 0;
//	_update_save_details(hdd_path, mount);
	vita_SaveUmount(mount);

	show_message("Encrypted save copied successfully!\n%s/%s", save->title_id, save->dir_name);
	return;
}

static void copyKeystone(int import)
{
	char path_data[256];
	char path_save[256];

	snprintf(path_save, sizeof(path_save), "%ssce_sys/keystone", selected_entry->path);
	snprintf(path_data, sizeof(path_data), APOLLO_USER_PATH "%s/keystone", apollo_config.user_id, selected_entry->title_id);
	mkdirs(path_data);

	LOG("Copy '%s' <-> '%s'...", path_save, path_data);

	if (copy_file(import ? path_data : path_save, import ? path_save : path_data) == SUCCESS)
		show_message("Keystone successfully copied to:\n%s", import ? path_save : path_data);
	else
		show_message("Error! Keystone couldn't be copied");
}

static int webReqHandler(const dWebRequest_t* req, char* outfile)
{
	list_node_t *node;
	save_entry_t *item;
	list_t *list = ((void**)selected_entry->dir_name)[0];

	// http://ps3-ip:8080/
	if (strcmp(req->resource, "/") == 0)
	{
		uint64_t hash[2];
		md5_context ctx;

		md5_starts(&ctx);
		for (node = list_head(list); (item = list_get(node)); node = list_next(node))
			md5_update(&ctx, (uint8_t*) item->name, strlen(item->name));

		md5_finish(&ctx, (uint8_t*) hash);
		snprintf(outfile, BUFSIZ, APOLLO_LOCAL_CACHE "web%08lx%08lx.html", hash[0], hash[1]);

		if (file_exists(outfile) == SUCCESS)
			return 1;

		FILE* f = fopen(outfile, "w");
		if (!f)
			return 0;

		fprintf(f, "<html><head><meta charset=\"UTF-8\"><style>h1, h2 { font-family: arial; } table { border-collapse: collapse; margin: 25px 0; font-size: 0.9em; font-family: sans-serif; min-width: 400px; box-shadow: 0 0 20px rgba(0, 0, 0, 0.15); } table thead tr { background-color: #009879; color: #ffffff; text-align: left; } table th, td { padding: 12px 15px; } table tbody tr { border-bottom: 1px solid #dddddd; } table tbody tr:nth-of-type(even) { background-color: #f3f3f3; } table tbody tr:last-of-type { border-bottom: 2px solid #009879; }</style>");
		fprintf(f, "<script language=\"javascript\">document.addEventListener(\"DOMContentLoaded\",function(){var e;if(\"IntersectionObserver\"in window){e=document.querySelectorAll(\".lazy\");var n=new IntersectionObserver(function(e,t){e.forEach(function(e){if(e.isIntersecting){var t=e.target;t.src=t.dataset.src,t.classList.remove(\"lazy\"),n.unobserve(t)}})});e.forEach(function(e){n.observe(e)})}else{var t;function r(){t&&clearTimeout(t),t=setTimeout(function(){var n=window.pageYOffset;e.forEach(function(e){e.offsetTop<window.innerHeight+n&&(e.src=e.dataset.src,e.classList.remove(\"lazy\"))}),0==e.length&&(document.removeEventListener(\"scroll\",r),window.removeEventListener(\"resize\",r),window.removeEventListener(\"orientationChange\",r))},20)}e=document.querySelectorAll(\".lazy\"),document.addEventListener(\"scroll\",r),window.addEventListener(\"resize\",r),window.addEventListener(\"orientationChange\",r)}});</script>");
		fprintf(f, "<title>Apollo Save Tool</title></head><body><h1>.:: Apollo Save Tool</h1><h2>Index of %s</h2><table><thead><tr><th>Name</th><th>Icon</th><th>Title ID</th><th>Folder</th><th>Location</th></tr></thead><tbody>", selected_entry->path);

		int i = 0;
		for (node = list_head(list); (item = list_get(node)); node = list_next(node), i++)
		{
			if (item->type == FILE_TYPE_MENU || !(item->flags & (SAVE_FLAG_PSV|SAVE_FLAG_PSP)) || item->flags & SAVE_FLAG_LOCKED)
				continue;

			fprintf(f, "<tr><td><a href=\"/zip/%08x/%s_%s.zip\">%s</a></td>", i, item->title_id, item->dir_name, item->name);
			fprintf(f, "<td><img class=\"lazy\" data-src=\"");

			if (item->flags & SAVE_FLAG_PSP)
				fprintf(f, "/icon/%08x/ICON0.PNG\" width=\"144\" height=\"80", i);
			else
				fprintf(f, "/icon/%s/icon0.png\" width=\"128\" height=\"128", item->title_id);

			fprintf(f, "\" alt=\"%s\"></td>", item->name);
			fprintf(f, "<td>%s</td>", item->title_id);
			fprintf(f, "<td>%s</td>", item->dir_name);
			fprintf(f, "<td>%.4s</td></tr>", item->path);
		}

		fprintf(f, "</tbody></table></body></html>");
		fclose(f);
		return 1;
	}

	// http://ps3-ip:8080/zip/00000000/CUSA12345_DIR-NAME.zip
	if (wildcard_match(req->resource, "/zip/\?\?\?\?\?\?\?\?/\?\?\?\?\?\?\?\?\?_*.zip"))
	{
		char mount[32];
		char *base, *path;
		int id = 0;

		snprintf(outfile, BUFSIZ, "%s%s", APOLLO_LOCAL_CACHE, req->resource + 14);
		sscanf(req->resource + 5, "%08x", &id);
		item = list_get_item(list, id);

		if (item->flags & SAVE_FLAG_PSV && item->flags & SAVE_FLAG_HDD)
			if (!vita_SaveMount(item, mount))
				return 0;

		base = strdup(item->path);
		path = strdup(item->path);
		*strrchr(base, '/') = 0;
		*strrchr(base, '/') = 0;

		id = zip_directory(base, path, outfile);
		if (item->flags & SAVE_FLAG_PSV && item->flags & SAVE_FLAG_HDD)
			vita_SaveUmount(mount);

		free(base);
		free(path);
		return id;
	}

	// http://ps3-ip:8080/icon/CUSA12345-DIR-NAME/sce_sys/icon0.png
	if (wildcard_match(req->resource, "/icon/\?\?\?\?\?\?\?\?/ICON0.PNG"))
	{
		int id = 0;

		sscanf(req->resource + 6, "%08x", &id);
		item = list_get_item(list, id);
		snprintf(outfile, BUFSIZ, "%sICON0.PNG", item->path);

		return (file_exists(outfile) == SUCCESS);
	}

	// http://ps3-ip:8080/icon/CUSA12345/DIR-NAME_icon0.png
	if (wildcard_match(req->resource, "/icon/\?\?\?\?\?\?\?\?\?/icon0.png"))
	{
		snprintf(outfile, BUFSIZ, PSV_ICONS_PATH_HDD, req->resource + 6);
		return (file_exists(outfile) == SUCCESS);
	}

	return 0;
}

static void enableWebServer(const save_entry_t* save, int port)
{
	LOG("Starting local web server for '%s'...", save->path);

	if (web_start(port, webReqHandler))
	{
		show_message("Web Server listening on port %d.\nPress OK to stop the Server.", port);
		web_stop();
	}
	else show_message("Error starting Web Server!");
}

static void copyAllSavesUSB(const save_entry_t* save, const char* dst_path, int all)
{
	char copy_path[256];
	char save_path[256];
	char mount[32];
	uint64_t progress = 0;
	list_node_t *node;
	save_entry_t *item;
	list_t *list = ((void**)save->dir_name)[0];

	if (!list || mkdirs(dst_path) != SUCCESS)
	{
		show_message("Error! Folder is not available:\n%s", dst_path);
		return;
	}

	init_progress_bar("Copying all saves...");

	LOG("Copying all saves to '%s'...", dst_path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (item->type != FILE_TYPE_PSV || !(all || item->flags & SAVE_FLAG_SELECTED) || !vita_SaveMount(item, mount))
			continue;

		snprintf(save_path, sizeof(save_path), APOLLO_SANDBOX_PATH, item->dir_name);
		snprintf(copy_path, sizeof(copy_path), "%s%08x_%s_%s/", dst_path, apollo_config.user_id, item->title_id, item->dir_name);

		LOG("Copying <%s> to %s...", save_path, copy_path);
		copy_directory(save_path, save_path, copy_path);

		vita_SaveUmount(mount);
	}

	end_progress_bar();
	show_message("All Saves copied to:\n%s", dst_path);
}

static void exportFolder(const char* src_path, const char* exp_path, const char* msg)
{
	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", exp_path);
		return;
	}

	init_loading_screen(msg);

    LOG("Copying <%s> to %s...", src_path, exp_path);
	copy_directory(src_path, src_path, exp_path);

	stop_loading_screen();
	show_message("Files successfully copied to:\n%s", exp_path);
}
/*
void exportLicensesRap(const char* fname, uint8_t dest)
{
	DIR *d;
	struct dirent *dir;
	char lic_path[256];
	char exp_path[256];
	char msg[128] = "Exporting user licenses...";

	if (dest <= MAX_USB_DEVICES)
		snprintf(exp_path, sizeof(exp_path), EXPORT_RAP_PATH_USB, dest);
	else
		snprintf(exp_path, sizeof(exp_path), EXPORT_RAP_PATH_HDD);

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", exp_path);
		return;
	}

	snprintf(lic_path, sizeof(lic_path), EXDATA_PATH_HDD, apollo_config.user_id);
	d = opendir(lic_path);
	if (!d)
		return;

    init_loading_screen(msg);

	LOG("Exporting RAPs from folder '%s'...", lic_path);
	while ((dir = readdir(d)) != NULL)
	{
		if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0 &&
			(!fname || (strcmp(dir->d_name, fname) == 0)) &&
			strcmp(strrchr(dir->d_name, '.'), ".rif") == 0)
		{
			LOG("Exporting %s", dir->d_name);
			snprintf(msg, sizeof(msg), "Exporting %.36s...", dir->d_name);
//			rif2rap((u8*) apollo_config.idps, lic_path, dir->d_name, exp_path);
		}
	}
	closedir(d);

    stop_loading_screen();
	show_message("Files successfully copied to:\n%s", exp_path);
}

void importLicenses(const char* fname, const char* exdata_path)
{
	DIR *d;
	struct dirent *dir;
	char lic_path[256];

	if (dir_exists(exdata_path) != SUCCESS)
	{
		show_message("Error! Import folder is not available:\n%s", exdata_path);
		return;
	}

	snprintf(lic_path, sizeof(lic_path), EXDATA_PATH_HDD, apollo_config.user_id);
	d = opendir(exdata_path);
	if (!d)
		return;

    init_loading_screen("Importing user licenses...");

	LOG("Importing RAPs from folder '%s'...", exdata_path);
	while ((dir = readdir(d)) != NULL)
	{
		if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0 &&
			(!fname || (strcmp(dir->d_name, fname)) == 0) &&
			strcasecmp(strrchr(dir->d_name, '.'), ".rap") == 0)
		{
			LOG("Importing %s", dir->d_name);
//			rap2rif((u8*) apollo_config.idps, exdata_path, dir->d_name, lic_path);
		}
	}
	closedir(d);

    stop_loading_screen();
	show_message("Files successfully copied to:\n%s", lic_path);
}
*/
static int apply_sfo_patches(sfo_patch_t* patch)
{
    code_entry_t* code;
    char in_file_path[256];
    char tmp_dir[SFO_DIRECTORY_SIZE];
    u8 tmp_psid[SFO_PSID_SIZE];
    list_node_t* node;

    if (selected_entry->flags & SAVE_FLAG_PSP)
        return 1;

    for (node = list_head(selected_entry->codes); (code = list_get(node)); node = list_next(node))
    {
        if (!code->activated || code->type != PATCH_SFO)
            continue;

        LOG("Active: [%s]", code->name);

        switch (code->codes[0])
        {
        case SFO_UNLOCK_COPY:
            if (selected_entry->flags & SAVE_FLAG_LOCKED)
                selected_entry->flags ^= SAVE_FLAG_LOCKED;

            patch->flags = SFO_PATCH_FLAG_REMOVE_COPY_PROTECTION;
            break;

        case SFO_CHANGE_ACCOUNT_ID:
            if (selected_entry->flags & SAVE_FLAG_OWNER)
                selected_entry->flags ^= SAVE_FLAG_OWNER;

            sscanf(code->options->value[code->options->sel], "%lx", &patch->account_id);
            break;

        case SFO_REMOVE_PSID:
            bzero(tmp_psid, SFO_PSID_SIZE);
            patch->psid = tmp_psid;
            break;

        case SFO_CHANGE_TITLE_ID:
            patch->directory = strstr(selected_entry->path, selected_entry->title_id);
            snprintf(in_file_path, sizeof(in_file_path), "%s", selected_entry->path);
            strncpy(tmp_dir, patch->directory, SFO_DIRECTORY_SIZE);

            strncpy(selected_entry->title_id, code->options[0].name[code->options[0].sel], 9);
            strncpy(patch->directory, selected_entry->title_id, 9);
            strncpy(tmp_dir, selected_entry->title_id, 9);
            *strrchr(tmp_dir, '/') = 0;
            patch->directory = tmp_dir;

            LOG("Moving (%s) -> (%s)", in_file_path, selected_entry->path);
            rename(in_file_path, selected_entry->path);
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
		if (!code->activated || (code->type != PATCH_GAMEGENIE && code->type != PATCH_BSD))
			continue;

		LOG("Active code: [%s]", code->name);

		if (strrchr(code->file, '\\'))
			filename = strrchr(code->file, '\\')+1;
		else
			filename = code->file;

		if (strchr(filename, '*'))
			filename = code->options[0].name[code->options[0].sel];

		if (strstr(code->file, "~extracted\\"))
			snprintf(tmpfile, sizeof(tmpfile), "%s[%s]%s", APOLLO_LOCAL_CACHE, entry->title_id, filename);
		else
		{
			snprintf(tmpfile, sizeof(tmpfile), "%s%s", entry->path, filename);

			if (entry->flags & SAVE_FLAG_PSP && !psp_is_decrypted(decrypted_files, filename))
			{
				if (get_psp_save_key(entry, key) && psp_DecryptSavedata(tmpfile, key))
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

		if (!apply_cheat_patch_code(tmpfile, entry->title_id, code, APOLLO_LOCAL_CACHE))
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

static void resignSave(sfo_patch_t* patch)
{
    LOG("Resigning save '%s'...", selected_entry->name);

    if (!apply_sfo_patches(patch))
        show_message("Error! Account changes couldn't be applied");

    LOG("Applying cheats to '%s'...", selected_entry->name);
    if (!apply_cheat_patches(selected_entry))
        show_message("Error! Cheat codes couldn't be applied");

    show_message("Save %s successfully modified!", selected_entry->title_id);
}

static void resignAllSaves(const save_entry_t* save, int all)
{
	char sfoPath[256];
	int err_count = 0;
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
		if (item->type != FILE_TYPE_PSV || (item->flags & SAVE_FLAG_LOCKED) || !(all || item->flags & SAVE_FLAG_SELECTED))
			continue;

		snprintf(sfoPath, sizeof(sfoPath), "%s" "sce_sys/param.sfo", item->path);
		if (file_exists(sfoPath) != SUCCESS)
			continue;

		LOG("Patching SFO '%s'...", sfoPath);
		err_count += patch_sfo(sfoPath, &patch);
	}

	end_progress_bar();

	if (err_count)
		show_message("Error: %d Saves couldn't be resigned", err_count);
	else
		show_message("All saves successfully resigned!");
}
/*
int apply_trophy_account()
{
	char sfoPath[256];
	char account_id[SFO_ACCOUNT_ID_SIZE+1];

	snprintf(account_id, sizeof(account_id), "%*lx", SFO_ACCOUNT_ID_SIZE, apollo_config.account_id);
	if (!apollo_config.account_id)
		memset(account_id, 0, SFO_ACCOUNT_ID_SIZE);

	snprintf(sfoPath, sizeof(sfoPath), "%s" "PARAM.SFO", selected_entry->path);

	patch_sfo_trophy(sfoPath, account_id);
//	patch_trophy_account(selected_entry->path, account_id);

	return 1;
}

int apply_trophy_patches()
{
	int ret = 1;
	uint32_t trophy_id;
	code_entry_t* code;
	list_node_t* node;

	init_loading_screen("Applying changes...");

	for (node = list_head(selected_entry->codes); (code = list_get(node)); node = list_next(node))
	{
		if (!code->activated || (code->type != PATCH_TROP_UNLOCK && code->type != PATCH_TROP_LOCK))
			continue;

		trophy_id = *(uint32_t*)(code->file);
    	LOG("Active code: [%d] '%s'", trophy_id, code->name);

if (0)
//		if (!apply_trophy_patch(selected_entry->path, trophy_id, (code->type == PATCH_TROP_UNLOCK)))
		{
			LOG("Error: failed to apply (%s)", code->name);
			ret = 0;
		}

		if (code->type == PATCH_TROP_UNLOCK)
		{
			code->type = PATCH_TROP_LOCK;
			code->name[1] = ' ';
		}
		else
		{
			code->type = PATCH_TROP_UNLOCK;
			code->name[1] = CHAR_TAG_LOCKED;
		}

		code->activated = 0;
	}

	stop_loading_screen();

	return ret;
}

void resignTrophy()
{
	LOG("Decrypting TROPTRNS.DAT ...");
if (0)
//	if (!decrypt_trophy_trns(selected_entry->path))
	{
		LOG("Error: failed to decrypt TROPTRNS.DAT");
		return;
	}

    if (!apply_trophy_account())
        show_message("Error! Account changes couldn't be applied");

    LOG("Applying trophy changes to '%s'...", selected_entry->name);
    if (!apply_trophy_patches())
        show_message("Error! Trophy changes couldn't be applied");

	LOG("Encrypting TROPTRNS.DAT ...");
if (0)
//	if (!encrypt_trophy_trns(selected_entry->path))
	{
		LOG("Error: failed to encrypt TROPTRNS.DAT");
		return;
	}

    LOG("Resigning trophy '%s'...", selected_entry->name);

if (0)
//    if (!pfd_util_init((u8*) apollo_config.idps, apollo_config.user_id, selected_entry->title_id, selected_entry->path) ||
//        (pfd_util_process(PFD_CMD_UPDATE, 0) != SUCCESS))
        show_message("Error! Trophy %s couldn't be resigned", selected_entry->title_id);
    else
        show_message("Trophy %s successfully modified!", selected_entry->title_id);

//    pfd_util_end();

	if ((file_exists("/dev_hdd0/mms/db.err") != SUCCESS) && show_dialog(1, "Schedule Database rebuild on next boot?"))
	{
		LOG("Creating db.err file for database rebuild...");
		write_buffer("/dev_hdd0/mms/db.err", (u8*) "\x00\x00\x03\xE9", 4);
	}
}
*/
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
		show_message("Error! No game decryption key available for %s", entry->title_id);
		return;
	}

	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s/", apollo_config.user_id, entry->dir_name);
	mkdirs(path);

	LOG("Decrypt '%s%s' to '%s'...", entry->path, filename, path);

	if (_copy_save_file(entry->path, path, filename))
	{
		strlcat(path, filename, sizeof(path));
		if (entry->flags & SAVE_FLAG_PSP && !psp_DecryptSavedata(path, key))
			show_message("Error! File %s couldn't be exported", filename);

		show_message("File successfully exported to:\n%s", path);
	}
	else
		show_message("Error! File %s couldn't be exported", filename);
}

static void encryptSaveFile(const save_entry_t* entry, const char* filename)
{
	char path[256];
	uint8_t key[16];

	if (entry->flags & SAVE_FLAG_PSP && !get_psp_save_key(entry, key))
	{
		show_message("Error! No game decryption key available for %s", entry->title_id);
		return;
	}

	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s/%s", apollo_config.user_id, entry->dir_name, filename);
	if (file_exists(path) != SUCCESS)
	{
		show_message("Error! Can't find decrypted save-game file:\n%s", path);
		return;
	}
	strrchr(path, '/')[1] = 0;

	LOG("Encrypt '%s%s' to '%s'...", path, filename, entry->path);

	if (_copy_save_file(path, entry->path, filename))
	{
		if (entry->flags & SAVE_FLAG_PSP && !psp_EncryptSavedata(entry->path, filename, key))
			show_message("Error! File %s couldn't be imported", filename);

		show_message("File successfully imported to:\n%s%s", entry->path, filename);
	}
	else
		show_message("Error! File %s couldn't be imported", filename);
}

void execCodeCommand(code_entry_t* code, const char* codecmd)
{
	char mount[32];

	if (selected_entry->flags & SAVE_FLAG_PSV && selected_entry->flags & SAVE_FLAG_HDD)
		if (!vita_SaveMount(selected_entry, mount))
		{
			LOG("Error Mounting Save! Check Save Mount Patches");
			return;
		}

	switch (codecmd[0])
	{
		case CMD_DECRYPT_FILE:
			decryptSaveFile(selected_entry, code->options[0].name[code->options[0].sel]);
			code->activated = 0;
			break;

		case CMD_DOWNLOAD_USB:
			downloadSave(selected_entry, code->file, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_EXPORT_ZIP_USB:
			zipSave(codecmd[1] ? EXPORT_PATH_IMC0 : EXPORT_PATH_UMA0);
			code->activated = 0;
			break;

		case CMD_EXPORT_ZIP_HDD:
			zipSave(APOLLO_PATH);
			code->activated = 0;
			break;

		case CMD_COPY_SAVE_USB:
			copySave(selected_entry, codecmd[1] ? SAVES_PATH_IMC0 : SAVES_PATH_UMA0);
			code->activated = 0;
			break;

		case CMD_COPY_SAVE_HDD:
			copySaveHDD(selected_entry);
			code->activated = 0;
			break;

		case CMD_EXP_KEYSTONE:
		case CMD_IMP_KEYSTONE:
			copyKeystone(codecmd[0] == CMD_IMP_KEYSTONE);
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
/*
		case CMD_EXP_TROPHY_USB:
			copySave(selected_entry, codecmd[1] ? TROPHY_PATH_USB1 : TROPHY_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_ZIP_TROPHY_USB:
			exportTrophiesZip(codecmd[1] ? EXPORT_PATH_USB1 : EXPORT_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_COPY_TROPHIES_USB:
			exportFolder(selected_entry->path, codecmd[1] ? TROPHY_PATH_USB1 : TROPHY_PATH_USB0, "Copying trophies...");
			code->activated = 0;
			break;
*/
		case CMD_COPY_SAVES_USB:
		case CMD_COPY_ALL_SAVES_USB:
			copyAllSavesUSB(selected_entry, codecmd[1] ? SAVES_PATH_IMC0 : SAVES_PATH_UMA0, codecmd[0] == CMD_COPY_ALL_SAVES_USB);
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
			{
				sfo_patch_t patch = {
					.flags = 0,
					.user_id = apollo_config.user_id,
//					.psid = (u8*) apollo_config.psid,
					.directory = NULL,
					.account_id = apollo_config.account_id,
				};

				resignSave(&patch);
			}
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

		case CMD_RUN_WEBSERVER:
			enableWebServer(selected_entry, 8080);
			code->activated = 0;
			break;

		case CMD_IMPORT_DATA_FILE:
			encryptSaveFile(selected_entry, code->options[0].name[code->options[0].sel]);
			code->activated = 0;
			break;

		case CMD_COPY_PFS:
			copySavePFS(selected_entry);
			code->activated = 0;
			break;

		case CMD_EXTRACT_ARCHIVE:
			extractArchive(code->file);
			code->activated = 0;
			break;

		default:
			break;
	}

	if (selected_entry->flags & SAVE_FLAG_PSV && selected_entry->flags & SAVE_FLAG_HDD)
		vita_SaveUmount(mount);

	return;
}
