#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <psp2/appmgr.h>
#include <libxml/parser.h>

#include "saves.h"
#include "common.h"
#include "sfo.h"
#include "settings.h"
#include "utils.h"
#include "sqlite3.h"
#include "vitashell_user.h"
#include "ps1card.h"

#define UTF8_CHAR_STAR		"\xE2\x98\x85"

#define CHAR_ICON_VMC		"\x02"
#define CHAR_ICON_NET		"\x09"
#define CHAR_ICON_ZIP		"\x0C"
#define CHAR_ICON_COPY		"\x0B"
#define CHAR_ICON_SIGN		"\x06"
#define CHAR_ICON_USER		"\x07"
#define CHAR_ICON_LOCK		"\x08"
#define CHAR_ICON_WARN		"\x0F"

#define MAX_MOUNT_POINT_LENGTH 16

int sqlite_init(void);

static char pfs_mount_point[MAX_MOUNT_POINT_LENGTH];
static const int known_pfs_ids[] = { 0x6E, 0x12E, 0x12F, 0x3ED };

static sqlite3* open_sqlite_db(const char* db_path)
{
	sqlite3 *db;

	// initialize the SceSqlite rw_vfs
	if (sqlite_init() != SQLITE_OK)
	{
		LOG("Error sqlite init");
		return NULL;
	}

	LOG("Opening '%s'...", db_path);
	if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK)
	{
		LOG("Error db open: %s", sqlite3_errmsg(db));
		return NULL;
	}

	return db;
}

static int get_appdb_title(sqlite3* db, const char* titleid, char* name)
{
	sqlite3_stmt* res;

	if (!db)
		return 0;

	char* query = sqlite3_mprintf("SELECT titleId, title FROM tbl_appinfo_icon WHERE (titleId = %Q)", titleid);

	if (sqlite3_prepare_v2(db, query, -1, &res, NULL) != SQLITE_OK || sqlite3_step(res) != SQLITE_ROW)
	{
		LOG("Failed to fetch data: %s", sqlite3_errmsg(db));
		sqlite3_free(query);
		return 0;
	}

	strncpy(name, (const char*) sqlite3_column_text(res, 1), 0x80);
	sqlite3_free(query);

	return 1;
}

int vita_SaveUmount()
{
	if (pfs_mount_point[0] == 0)
		return 0;

	int umountErrorCode = sceAppMgrUmount(pfs_mount_point);	
	if (umountErrorCode < 0)
	{
		LOG("UMOUNT_ERROR (%X)", umountErrorCode);
		notification("Warning! Save couldn't be unmounted!");
		return 0;
	}
	pfs_mount_point[0] = 0;

	return (umountErrorCode == SUCCESS);
}

int vita_SaveMount(const save_entry_t *save)
{
	char klicensee[0x10];
	ShellMountIdArgs args;

	memset(klicensee, 0, sizeof(klicensee));

	args.process_titleid = "NP0APOLLO";
	args.path = save->path;
	args.desired_mount_point = NULL;
	args.klicensee = klicensee;
	args.mount_point = pfs_mount_point;

	for (int i = 0; i < countof(known_pfs_ids); i++)
	{
		args.id = known_pfs_ids[i];
		if (shellUserMountById(&args) < 0)
			continue;

		LOG("[%s] '%s' mounted (%s)", save->title_id, pfs_mount_point, save->path);
		return 1;
	}

	int mountErrorCode = sceAppMgrGameDataMount(save->path, 0, 0, pfs_mount_point);
	if (mountErrorCode < 0)
	{
		LOG("ERROR (%X): can't mount '%s/%s'", mountErrorCode, save->title_id, save->dir_name);
		return 0;
	}

	LOG("[%s] '/%s' mounted (%s)", save->title_id, pfs_mount_point, save->path);
	return 1;
}

int orbis_UpdateSaveParams(const char* mountPath, const char* title, const char* subtitle, const char* details)
{
	/*
	OrbisSaveDataParam saveParams;
	OrbisSaveDataMountPoint mount;

	memset(&saveParams, 0, sizeof(OrbisSaveDataParam));
	memset(&mount, 0, sizeof(OrbisSaveDataMountPoint));

	strncpy(mount.data, mountPath, sizeof(mount.data));
	strncpy(saveParams.title, title, ORBIS_SAVE_DATA_TITLE_MAXSIZE);
	strncpy(saveParams.subtitle, subtitle, ORBIS_SAVE_DATA_SUBTITLE_MAXSIZE);
	strncpy(saveParams.details, details, ORBIS_SAVE_DATA_DETAIL_MAXSIZE);
	saveParams.mtime = time(NULL);

	int32_t setParamResult = sceSaveDataSetParam(&mount, ORBIS_SAVE_DATA_PARAM_TYPE_ALL, &saveParams, sizeof(OrbisSaveDataParam));
	if (setParamResult < 0) {
		LOG("sceSaveDataSetParam error (%X)", setParamResult);
		return 0;
	}

	return (setParamResult == SUCCESS);
*/ return 0;
}

/*
 * Function:		endsWith()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Checks to see if a ends with b
 * Arguments:
 *	a:				String
 *	b:				Potential end
 * Return:			pointer if true, NULL if false
 */
static char* endsWith(const char * a, const char * b)
{
	int al = strlen(a), bl = strlen(b);
    
	if (al < bl)
		return NULL;

	a += (al - bl);
	while (*a)
		if (toupper(*a++) != toupper(*b++)) return NULL;

	return (char*) (a - bl);
}

/*
 * Function:		readFile()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		reads the contents of a file into a new buffer
 * Arguments:
 *	path:			Path to file
 * Return:			Pointer to the newly allocated buffer
 */
char * readTextFile(const char * path, long* size)
{
	FILE *f = fopen(path, "rb");

	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (fsize <= 0)
	{
		fclose(f);
		return NULL;
	}

	char * string = malloc(fsize + 1);
	fread(string, fsize, 1, f);
	fclose(f);

	string[fsize] = 0;
	if (size)
		*size = fsize;

	return string;
}

static code_entry_t* _createCmdCode(uint8_t type, const char* name, char code)
{
	code_entry_t* entry = (code_entry_t *)calloc(1, sizeof(code_entry_t));
	entry->type = type;
	entry->name = strdup(name);
	asprintf(&entry->codes, "%c", code);

	return entry;
}

static option_entry_t* _initOptions(int count)
{
	option_entry_t* options = (option_entry_t*)calloc(count, sizeof(option_entry_t));

	for(int i = 0; i < count; i++)
	{
		options[i].sel = -1;
		options[i].opts = list_alloc();
	}

	return options;
}

static option_entry_t* _createOptions(int count, const char* name, char value)
{
	option_value_t* optval;
	option_entry_t* options = _initOptions(count);

	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "%s (%s)", name, UMA0_PATH);
	asprintf(&optval->value, "%c%c", value, STORAGE_UMA0);
	list_append(options[0].opts, optval);

	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "%s (%s)", name, UX0_PATH);
	asprintf(&optval->value, "%c%c", value, STORAGE_UX0);
	list_append(options[0].opts, optval);

	if (dir_exists(IMC0_PATH) == SUCCESS)
	{
		optval = malloc(sizeof(option_value_t));
		asprintf(&optval->name, "%s (%s)", name, IMC0_PATH);
		asprintf(&optval->value, "%c%c", value, STORAGE_IMC0);
		list_append(options[0].opts, optval);
	}

	return options;
}

static save_entry_t* _createSaveEntry(uint16_t flag, const char* name)
{
	save_entry_t* entry = (save_entry_t *)calloc(1, sizeof(save_entry_t));
	entry->flags = flag;
	entry->name = strdup(name);

	return entry;
}

static void _walk_dir_list(const char* startdir, const char* inputdir, const char* mask, list_t* list)
{
	char fullname[256];	
	struct dirent *dirp;
	int len = strlen(startdir);
	DIR *dp = opendir(inputdir);

	if (!dp) {
		LOG("Failed to open input directory: '%s'", inputdir);
		return;
	}

	while ((dirp = readdir(dp)) != NULL)
	{
		if ((strcmp(dirp->d_name, ".")  == 0) || (strcmp(dirp->d_name, "..") == 0) || (strcmp(dirp->d_name, "sce_sys") == 0) ||
			(strcmp(dirp->d_name, "ICON0.PNG") == 0) || (strcmp(dirp->d_name, "PARAM.SFO") == 0) || (strcmp(dirp->d_name,"PIC1.PNG") == 0) ||
			(strcmp(dirp->d_name, "ICON1.PMF") == 0) || (strcmp(dirp->d_name, "SND0.AT3") == 0))
			continue;

		snprintf(fullname, sizeof(fullname), "%s%s", inputdir, dirp->d_name);

		if (dirp->d_stat.st_mode & SCE_S_IFDIR)
		{
			strcat(fullname, "/");
			_walk_dir_list(startdir, fullname, mask, list);
		}
		else if (wildcard_match_icase(dirp->d_name, mask))
		{
			//LOG("Adding file '%s'", fullname+len);
			list_append(list, strdup(fullname+len));
		}
	}
	closedir(dp);
}

static option_entry_t* _getFileOptions(const char* save_path, const char* mask, uint8_t is_cmd)
{
	char *filename;
	list_t* file_list;
	list_node_t* node;
	option_value_t* optval;
	option_entry_t* opt;

	LOG("Loading filenames {%s} from '%s'...", mask, save_path);

	file_list = list_alloc();
	_walk_dir_list(save_path, save_path, mask, file_list);

	if (!list_count(file_list))
	{
		is_cmd = 0;
		asprintf(&filename, CHAR_ICON_WARN " --- %s%s --- " CHAR_ICON_WARN, save_path, mask);
		list_append(file_list, filename);
	}

	opt = _initOptions(1);

	for (node = list_head(file_list); (filename = list_get(node)); node = list_next(node))
	{
		LOG("Adding '%s' (%s)", filename, mask);

		optval = malloc(sizeof(option_value_t));
		optval->name = filename;

		if (is_cmd)
			asprintf(&optval->value, "%c", is_cmd);
		else
			asprintf(&optval->value, "%s", mask);

		list_append(opt[0].opts, optval);
	}

	list_free(file_list);

	return opt;
}

static void _addBackupCommands(save_entry_t* item)
{
	code_entry_t* cmd;
	option_value_t* optval;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Apply Changes & Resign", CMD_RESIGN_SAVE);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " File Backup " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy save game", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(1, "Copy Save to Backup Storage", CMD_COPY_SAVE_USB);
	if (!(item->flags & SAVE_FLAG_HDD))
	{
		optval = malloc(sizeof(option_value_t));
		asprintf(&optval->name, "Copy Save to User Storage (ux0:%s/)", (item->flags & SAVE_FLAG_PSP) ? "pspemu":"user");
		asprintf(&optval->value, "%c", CMD_COPY_SAVE_HDD);
		list_append(cmd->options[0].opts, optval);
	}
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_ZIP " Export save game to Zip", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(1, "Export Zip to Backup Storage", CMD_EXPORT_ZIP_USB);
	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "Export Zip to User Storage (ux0:data/)");
	asprintf(&optval->value, "%c%c", CMD_EXPORT_ZIP_USB, STORAGE_UX0);
	list_append(cmd->options[0].opts, optval);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export decrypted save files", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _getFileOptions(item->path, "*", CMD_DECRYPT_FILE);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Import decrypted save files", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _getFileOptions(item->path, "*", CMD_IMPORT_DATA_FILE);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Hex Edit save game files", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _getFileOptions(item->path, "*", CMD_HEX_EDIT_FILE);
	list_append(item->codes, cmd);
}

static option_entry_t* _getSaveTitleIDs(const char* title_id)
{
	option_value_t* optval;
	option_entry_t* opt;
	char tmp[16];
	const char *ptr;
	const char *tid = NULL;//get_game_title_ids(title_id);

	if (!tid)
		tid = title_id;

	LOG("Adding TitleIDs=%s", tid);
	opt = _initOptions(1);

	ptr = tid;
	while (*ptr++)
	{
		if ((*ptr == '/') || (*ptr == 0))
		{
			memset(tmp, 0, sizeof(tmp));
			strncpy(tmp, tid, ptr - tid);
			optval = malloc(sizeof(option_value_t));
			asprintf(&optval->name, "%s", tmp);
			asprintf(&optval->value, "%c", SFO_CHANGE_TITLE_ID);
			list_append(opt[0].opts, optval);
			tid = ptr+1;
		}
	}

	return opt;
}

static void addVitaCommands(save_entry_t* save)
{
	code_entry_t* cmd;

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Keystone Backup " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Export Keystone", CMD_EXP_KEYSTONE);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Import Keystone", CMD_IMP_KEYSTONE);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Dump save Fingerprint", CMD_EXP_FINGERPRINT);
	list_append(save->codes, cmd);

	return;
}

static void add_vmp_commands(save_entry_t* save)
{
	char path[256];
	code_entry_t* cmd;

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Virtual Memory Card " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(save->codes, cmd);

	if (endsWith(save->path, ".VMP"))
	{
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Resign Memory Card", CMD_RESIGN_VMP);
		list_append(save->codes, cmd);
	}

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export Memory Card to .MCR", CMD_EXP_VMP2MCR);
	list_append(save->codes, cmd);

	snprintf(path, sizeof(path), CHAR_ICON_COPY " Import .MCR files to %s", strrchr(save->path, '/')+1);
	cmd = _createCmdCode(PATCH_COMMAND, path, CMD_CODE_NULL);
	cmd->options_count = 1;
	snprintf(path, sizeof(path), PS1_SAVES_PATH_HDD "%s/", save->title_id);
	cmd->options = _getFileOptions(path, "*.MCR", CMD_IMP_MCR2VMP);
	list_append(save->codes, cmd);

	return;
}

static void add_psp_commands(save_entry_t* item)
{
	code_entry_t* cmd;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_WARN " Delete Save Game", CMD_DELETE_SAVE);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Game Key Backup " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Dump binary Save-game Key", CMD_EXP_PSPKEY);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Export Save-game Key (text file)", CMD_DUMP_PSPKEY);
	list_append(item->codes, cmd);

	return;
}

option_entry_t* get_file_entries(const char* path, const char* mask)
{
	return _getFileOptions(path, mask, CMD_CODE_NULL);
}

/*
 * Function:		ReadLocalCodes()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Reads an entire NCL file into an array of code_entry
 * Arguments:
 *	path:			Path to ncl
 *	_count_count:	Pointer to int (set to the number of codes within the ncl)
 * Return:			Returns an array of code_entry, null if failed to load
 */
int ReadCodes(save_entry_t * save)
{
	code_entry_t * code;
	char filePath[256];
	char * buffer = NULL;

	save->codes = list_alloc();

	if (save->flags & SAVE_FLAG_PSV && save->flags & SAVE_FLAG_HDD && !vita_SaveMount(save))
	{
		code = _createCmdCode(PATCH_NULL, CHAR_ICON_WARN " --- Error Mounting Save! Check Save Mount Patches --- " CHAR_ICON_WARN, CMD_CODE_NULL);
		list_append(save->codes, code);
		return list_count(save->codes);
	}

	_addBackupCommands(save);

	if (save->flags & SAVE_FLAG_PSP)
		add_psp_commands(save);
	else
		addVitaCommands(save);

	snprintf(filePath, sizeof(filePath), APOLLO_DATA_PATH "%s.savepatch", save->title_id);
	if ((buffer = readTextFile(filePath, NULL)) == NULL)
		goto skip_end;

	code = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Cheats " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);	
	list_append(save->codes, code);

	code = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Raw Patch File", CMD_VIEW_RAW_PATCH);
	list_append(save->codes, code);

	LOG("Loading BSD codes '%s'...", filePath);
	load_patch_code_list(buffer, save->codes, &get_file_entries, save->path);
	free (buffer);

skip_end:
	if (save->flags & SAVE_FLAG_PSV && save->flags & SAVE_FLAG_HDD)
		vita_SaveUmount();

	LOG("Loaded %ld codes", list_count(save->codes));

	return list_count(save->codes);
}

static char* _get_xml_node_value(xmlNode * a_node, const xmlChar* node_name)
{
	xmlNode *cur_node = NULL;
	char *value = NULL;

	for (cur_node = a_node; cur_node && !value; cur_node = cur_node->next)
	{
		if (cur_node->type != XML_ELEMENT_NODE)
			continue;

		if (xmlStrcasecmp(cur_node->name, node_name) == 0)
		{
			value = (char*) xmlNodeGetContent(cur_node);
//			LOG("xml value=%s", value);
		}
	}

	return value;
}

static int get_usb_trophies(save_entry_t* item)
{
	DIR *d;
	struct dirent *dir;
	code_entry_t * cmd;
	char filePath[256];
	xmlDoc *doc = NULL;
	xmlNode *root_element = NULL;
	char *name, *commid;

	d = opendir(item->path);
	if (!d)
		return 0;

	item->codes = list_alloc();
	while ((dir = readdir(d)) != NULL)
	{
		if (!(dir->d_stat.st_mode & SCE_S_IFDIR) || strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
			continue;

		snprintf(filePath, sizeof(filePath), "%s%s/conf/TROP.SFM", item->path, dir->d_name);
		LOG("Reading %s...", filePath);

		/*parse the file and get the DOM */
		doc = xmlParseFile(filePath);
		if (!doc)
		{
			LOG("XML: could not parse file %s", filePath);
			continue;
		}

		/*Get the root element node */
		root_element = xmlDocGetRootElement(doc);
		name = _get_xml_node_value(root_element->children, BAD_CAST "title-name");
		commid = _get_xml_node_value(root_element->children, BAD_CAST "npcommid");
		snprintf(filePath, sizeof(filePath), "%s (%s)", name, commid);

		cmd = _createCmdCode(PATCH_COMMAND, filePath, CMD_IMP_TROPHY_HDD);
		cmd->flags = APOLLO_CODE_FLAG_PARENT;
		cmd->file = strdup(dir->d_name);

		/*free the document */
		xmlFreeDoc(doc);
		xmlCleanupParser();

		LOG("[%s] name '%s'", cmd->file, cmd->name);
		list_append(item->codes, cmd);
	}
	closedir(d);

	return list_count(item->codes);
}

int ReadTrophies(save_entry_t * game)
{
	int trop_count = 0;
	code_entry_t * trophy;
	char query[256];
	sqlite3 *db;
	sqlite3_stmt *res;

	// Import trophies from USB
	if (game->type == FILE_TYPE_MENU)
		return get_usb_trophies(game);

	snprintf(query, sizeof(query), TROPHY_PATH_HDD "db/trophy_local.db", apollo_config.user_id);
	if ((db = open_sqlite_db(query)) == NULL)
		return 0;

	game->codes = list_alloc();
/*
	trophy = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Apply Changes & Resign Trophy Set", CMD_RESIGN_TROPHY);
	list_append(game->codes, trophy);
*/
	trophy = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Backup Trophy Set to Backup Storage", CMD_CODE_NULL);
	trophy->file = strdup(game->path);
	trophy->options_count = 1;
	trophy->options = _createOptions(1, "Copy Trophy to Backup Storage", CMD_EXP_TROPHY_USB);
	list_append(game->codes, trophy);

	trophy = _createCmdCode(PATCH_COMMAND, CHAR_ICON_ZIP " Export Trophy Set to Zip", CMD_CODE_NULL);
	trophy->file = strdup(game->path);
	trophy->options_count = 1;
	trophy->options = _createOptions(1, "Save .Zip to Backup Storage", CMD_ZIP_TROPHY_USB);
	list_append(game->codes, trophy);

	trophy = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Trophies " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(game->codes, trophy);

	snprintf(query, sizeof(query), "SELECT title_id, npcommid, title, description, grade, unlocked, id FROM tbl_trophy_flag WHERE title_id = %d", game->blocks);

	if (sqlite3_prepare_v2(db, query, -1, &res, NULL) != SQLITE_OK)
	{
		LOG("Failed to fetch data: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 0;
	}

	while (sqlite3_step(res) == SQLITE_ROW)
	{
		snprintf(query, sizeof(query), "   %s", sqlite3_column_text(res, 2));
		trophy = _createCmdCode(PATCH_NULL, query, CMD_CODE_NULL);

		asprintf(&trophy->codes, "%s\n", sqlite3_column_text(res, 3));

		switch (sqlite3_column_int(res, 4))
		{
		case 4:
			trophy->name[0] = CHAR_TRP_BRONZE;
			break;

		case 3:
			trophy->name[0] = CHAR_TRP_SILVER;
			break;

		case 2:
			trophy->name[0] = CHAR_TRP_GOLD;
			break;

		case 1:
			trophy->name[0] = CHAR_TRP_PLATINUM;
			break;

		default:
			break;
		}

		trop_count = sqlite3_column_int(res, 6);
		trophy->file = malloc(sizeof(trop_count));
		memcpy(trophy->file, &trop_count, sizeof(trop_count));

		if (!sqlite3_column_int(res, 5))
			trophy->name[1] = CHAR_TAG_LOCKED;

		// if trophy has been synced, we can't allow changes
		if (0)
			trophy->name[1] = CHAR_TRP_SYNC;
		else
			trophy->type = (sqlite3_column_int(res, 5) ? PATCH_TROP_LOCK : PATCH_TROP_UNLOCK);

		LOG("Trophy=%d [%d] '%s' (%s)", trop_count, trophy->type, trophy->name, trophy->codes);
		list_append(game->codes, trophy);
	}

	sqlite3_finalize(res);
	sqlite3_close(db);

	return list_count(game->codes);
}

static void add_vmc_import_saves(list_t* list, const char* path, const char* folder)
{
	code_entry_t * cmd;
	DIR *d;
	struct dirent *dir;
	char psvPath[256];

	snprintf(psvPath, sizeof(psvPath), "%s%s", path, folder);
	d = opendir(psvPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (!endsWith(dir->d_name, ".PSV") && !endsWith(dir->d_name, ".MCS") && !endsWith(dir->d_name, ".PSX") &&
			!endsWith(dir->d_name, ".PS1") && !endsWith(dir->d_name, ".MCB") && !endsWith(dir->d_name, ".PDA"))
			continue;

		snprintf(psvPath, sizeof(psvPath), "%s %s", CHAR_ICON_COPY, dir->d_name);
		cmd = _createCmdCode(PATCH_COMMAND, psvPath, CMD_IMP_VMCSAVE);
		asprintf(&cmd->file, "%s%s%s", path, folder, dir->d_name);
		cmd->codes[1] = FILE_TYPE_PS1;
		list_append(list, cmd);

		LOG("[%s] F(%X) name '%s'", cmd->file, cmd->flags, cmd->name+2);
	}

	closedir(d);
}

static void read_vmc_files(const char* path, list_t* list)
{
	save_entry_t *item;
	DIR *d;
	struct dirent *dir;
	char vmcPath[256];

	snprintf(vmcPath, sizeof(vmcPath), PS1VMC_PATH_USB, USER_STORAGE_DEV);
	d = opendir(vmcPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (!endsWith(dir->d_name, ".VMP") && !endsWith(dir->d_name, ".MCR") && !endsWith(dir->d_name, ".GME") &&
			!endsWith(dir->d_name, ".VM1") && !endsWith(dir->d_name, ".MCD") && !endsWith(dir->d_name, ".VGS") &&
			!endsWith(dir->d_name, ".VMC") && !endsWith(dir->d_name, ".BIN") && !endsWith(dir->d_name, ".SRM"))
			continue;

		item = _createSaveEntry(SAVE_FLAG_PS1 | SAVE_FLAG_VMC, dir->d_name);
		item->type = FILE_TYPE_VMC;
		item->title_id = strdup("VMC");
		item->dir_name = strdup(PS1_SAVES_PATH_HDD);
		asprintf(&item->path, "%s%s", vmcPath, dir->d_name);
		list_append(list, item);

		LOG("[%s] F(%X) name '%s'", item->path, item->flags, item->name);
	}

	closedir(d);
}

int ReadVmcCodes(save_entry_t * save)
{
	code_entry_t * cmd;
	char filePath[256];

	save->codes = list_alloc();

	if (save->type == FILE_TYPE_MENU)
	{
		add_vmc_import_saves(save->codes, save->path, PS1_SAVES_PATH_USB);
		if (!list_count(save->codes))
		{
			list_free(save->codes);
			save->codes = NULL;
			return 0;
		}

		list_bubbleSort(save->codes, &sortCodeList_Compare);

		return list_count(save->codes);
	}

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_WARN " Delete Save Game", CMD_DELETE_SAVE);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Save Backup " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save game to MCS format", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(1, "Copy Save to Mass Storage", CMD_EXP_VMCSAVE);
	cmd->options[0].id = PS1SAVE_MCS;
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save game to PSV format", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(1, "Copy Save to Mass Storage", CMD_EXP_VMCSAVE);
	cmd->options[0].id = PS1SAVE_PSV;
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save game to PSX format", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(1, "Copy Save to Mass Storage", CMD_EXP_VMCSAVE);
	cmd->options[0].id = PS1SAVE_AR;
	list_append(save->codes, cmd);

	LOG("Loaded %ld codes", list_count(save->codes));

	return list_count(save->codes);
}

/*
 * Function:		ReadOnlineSaves()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Downloads an entire NCL file into an array of code_entry
 * Arguments:
 *	filename:		File name ncl
 *	_count_count:	Pointer to int (set to the number of codes within the ncl)
 * Return:			Returns an array of code_entry, null if failed to load
 */
int ReadOnlineSaves(save_entry_t * game)
{
	code_entry_t* item;
	option_value_t* optval;
	char path[256];
	snprintf(path, sizeof(path), APOLLO_LOCAL_CACHE "%s.txt", game->title_id);

	if (file_exists(path) == SUCCESS && strcmp(apollo_config.save_db, ONLINE_URL) == 0)
	{
		struct stat stats;
		stat(path, &stats);
		// re-download if file is +1 day old
		if ((stats.st_mtime + ONLINE_CACHE_TIMEOUT) < time(NULL))
			http_download(game->path, "saves.txt", path, 1);
	}
	else
	{
		if (!http_download(game->path, "saves.txt", path, 1))
			return 0;
	}

	long fsize;
	char *data = readTextFile(path, &fsize);
	if (!data)
		return 0;
	
	char *ptr = data;
	char *end = data + fsize;

	game->codes = list_alloc();

	while (ptr < end && *ptr)
	{
		const char* content = ptr;

		while (ptr < end && *ptr != '\n' && *ptr != '\r')
		{
			ptr++;
		}
		*ptr++ = 0;

		if (content[12] == '=')
		{
			snprintf(path, sizeof(path), CHAR_ICON_ZIP " %s", content + 13);
			item = _createCmdCode(PATCH_COMMAND, path, CMD_CODE_NULL);
			asprintf(&item->file, "%.12s", content);

			item->options_count = 1;
			item->options = _createOptions(1, "Download to Backup Storage", CMD_DOWNLOAD_USB);
			optval = malloc(sizeof(option_value_t));
			asprintf(&optval->name, "Download to User Storage (ux0:data/)");
			asprintf(&optval->value, "%c%c", CMD_DOWNLOAD_USB, STORAGE_UX0);
			list_append(item->options[0].opts, optval);
			list_append(game->codes, item);

			LOG("[%s%s] %s", game->path, item->file, item->name + 1);
		}

		if (ptr < end && *ptr == '\r')
		{
			ptr++;
		}
		if (ptr < end && *ptr == '\n')
		{
			ptr++;
		}
	}

	free(data);
	LOG("Loaded %d saves", list_count(game->codes));

	return (list_count(game->codes));
}

list_t * ReadBackupList(const char* userPath)
{
	save_entry_t * item;
	code_entry_t * cmd;
	list_t *list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_ZIP, CHAR_ICON_ZIP " Extract Archives (RAR, Zip, 7z)");
	asprintf(&item->path, "%s:data/", USER_STORAGE_DEV);
	item->title_id = strdup(item->path);
	item->type = FILE_TYPE_ZIP;
	list_append(list, item);

	item = _createSaveEntry(SAVE_FLAG_PSV, CHAR_ICON_COPY " Export NoNpDRM Licenses to zRIF");
	item->path = strdup(PSV_LICENSE_PATH);
	item->title_id = strdup(item->path);
	item->type = FILE_TYPE_RIF;
	list_append(list, item);

	item = _createSaveEntry(SAVE_FLAG_PSV, CHAR_ICON_COPY " Export NoNpDRM Licenses to zRIF");
	item->path = strdup("ux0:nonpdrm/license/");
	item->title_id = strdup("ux0:nonpdrm/");
	item->type = FILE_TYPE_RIF;
	list_append(list, item);

	item = _createSaveEntry(0, CHAR_ICON_NET " Network Tools (Downloader, Web Server)");
	asprintf(&item->path, "%s:data/", USER_STORAGE_DEV);
	item->type = FILE_TYPE_NET;
	list_append(list, item);

	item = _createSaveEntry(SAVE_FLAG_PSP, CHAR_ICON_COPY " Manage PSP Key Dumper tools");
	asprintf(&item->path, PSP_EMULATOR_PATH, "ux0");
	item->title_id = strdup(item->path);
	item->type = FILE_TYPE_PRX;
	list_append(list, item);

	item = _createSaveEntry(SAVE_FLAG_PSP, CHAR_ICON_COPY " Decompress .CSO to .ISO");
	asprintf(&item->path, PSP_EMULATOR_PATH "ISO/", USER_STORAGE_DEV);
	asprintf(&item->title_id, PSP_EMULATOR_PATH, USER_STORAGE_DEV);
	item->type = FILE_TYPE_CSO;
	list_append(list, item);

	item = _createSaveEntry(SAVE_FLAG_PSP, CHAR_ICON_COPY " Compress .ISO to .CSO");
	asprintf(&item->path, PSP_EMULATOR_PATH "ISO/", USER_STORAGE_DEV);
	asprintf(&item->title_id, PSP_EMULATOR_PATH, USER_STORAGE_DEV);
	item->type = FILE_TYPE_ISO;
	list_append(list, item);

	return list;
}

static size_t load_iso_files(save_entry_t * bup, int type)
{
	DIR *d;
	struct dirent *dir;
	code_entry_t * cmd;
	char tmp[256];

	bup->codes = list_alloc();
	LOG("Loading files from '%s'...", bup->path);

	d = opendir(bup->path);
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			if ((type == FILE_TYPE_ISO && !endsWith(dir->d_name, ".ISO")) || (type == FILE_TYPE_CSO && !endsWith(dir->d_name, ".CSO")))
				continue;

			snprintf(tmp, sizeof(tmp), CHAR_ICON_COPY " Convert %s", dir->d_name);
			cmd = _createCmdCode(PATCH_COMMAND, tmp, (type == FILE_TYPE_ISO) ? CMD_CONV_ISO2CSO : CMD_CONV_CSO2ISO);
			asprintf(&cmd->file, "%s%s", bup->path, dir->d_name);

			LOG("[%s] name '%s'", cmd->file, cmd->name +2);
			list_append(bup->codes, cmd);
		}
		closedir(d);
	}

	if (!list_count(bup->codes))
	{
		list_free(bup->codes);
		bup->codes = NULL;
		return 0;
	}

	LOG("%ld items loaded", list_count(bup->codes));

	return list_count(bup->codes);	
}

int ReadBackupCodes(save_entry_t * bup)
{
	code_entry_t * cmd;
	char tmp[256];

	switch(bup->type)
	{
	case FILE_TYPE_ZIP:
		break;

	case FILE_TYPE_ISO:
	case FILE_TYPE_CSO:
		return load_iso_files(bup, bup->type);

	case FILE_TYPE_NET:
		bup->codes = list_alloc();
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_NET " URL link Downloader (http, https, ftp, ftps)", CMD_URL_DOWNLOAD);
		list_append(bup->codes, cmd);
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_NET " Local Web Server (full system access)", CMD_NET_WEBSERVER);
		list_append(bup->codes, cmd);
		return list_count(bup->codes);

	case FILE_TYPE_PRX:
		bup->codes = list_alloc();
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " Install Save-game Key Dumper plugin", CMD_SETUP_PLUGIN);
		cmd->codes[1] = 1;
		list_append(bup->codes, cmd);
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Disable Save-game Key Dumper plugin", CMD_SETUP_PLUGIN);
		cmd->codes[1] = 0;
		list_append(bup->codes, cmd);
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " Install PSP FuseID Dumper application", CMD_SETUP_FUSEDUMP);
		list_append(bup->codes, cmd);

		return list_count(bup->codes);

	case FILE_TYPE_RIF:
		bup->codes = list_alloc();
		LOG("Getting .rifs from '%s'...", bup->path);

		char* filename;
		list_t* file_list = list_alloc();
		_walk_dir_list("", bup->path, "*.rif", file_list);

		if (!list_count(file_list))
		{
			asprintf(&filename, "%s --- No .rif licenses found ---", bup->path);
			list_append(file_list, filename);
		}

		for (list_node_t* node = list_head(file_list); (filename = list_get(node)); node = list_next(node))
		{
			snprintf(tmp, sizeof(tmp), CHAR_ICON_USER " %s", filename + strlen(bup->path));
			*strrchr(tmp, '/') = 0;
			cmd = _createCmdCode(PATCH_COMMAND, tmp, CMD_EXP_LIC_ZRIF);
			cmd->file = filename;
			list_append(bup->codes, cmd);

			LOG("[%s] name '%s'", cmd->file, cmd->name +2);
		}

		list_free(file_list);
		return list_count(bup->codes);

	default:
		return 0;
	}

	bup->codes = list_alloc();

	LOG("Loading files from '%s'...", bup->path);

	DIR *d;
	struct dirent *dir;
	d = opendir(bup->path);

	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			if (!(dir->d_stat.st_mode & SCE_S_IFREG) ||
				(!endsWith(dir->d_name, ".RAR") && !endsWith(dir->d_name, ".ZIP") && !endsWith(dir->d_name, ".7Z")))
				continue;

			snprintf(tmp, sizeof(tmp), CHAR_ICON_ZIP " Extract %s", dir->d_name);
			cmd = _createCmdCode(PATCH_COMMAND, tmp, CMD_EXTRACT_ARCHIVE);
			asprintf(&cmd->file, "%s%s", bup->path, dir->d_name);

			LOG("[%s] name '%s'", cmd->file, cmd->name +2);
			list_append(bup->codes, cmd);
		}
		closedir(d);
	}

	if (!list_count(bup->codes))
	{
		list_free(bup->codes);
		bup->codes = NULL;
		return 0;
	}

	LOG("%ld items loaded", list_count(bup->codes));

	return list_count(bup->codes);
}

/*
 * Function:		UnloadGameList()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Free entire array of game_entry
 * Arguments:
 *	list:			Array of game_entry to free
 *	count:			number of game entries
 * Return:			void
 */
void UnloadGameList(list_t * list)
{
	list_node_t *node, *nc, *no;
	save_entry_t *item;
	code_entry_t *code;
	option_value_t* optval;

	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		if (item->name)
		{
			free(item->name);
			item->name = NULL;
		}

		if (item->path)
		{
			free(item->path);
			item->path = NULL;
		}

		if (item->dir_name)
		{
			free(item->dir_name);
			item->dir_name = NULL;
		}

		if (item->title_id)
		{
			free(item->title_id);
			item->title_id = NULL;
		}
		
		if (item->codes)
		{
			for (nc = list_head(item->codes); (code = list_get(nc)); nc = list_next(nc))
			{
				if (code->codes)
				{
					free (code->codes);
					code->codes = NULL;
				}
				if (code->name)
				{
					free (code->name);
					code->name = NULL;
				}
				if (code->options && code->options_count > 0)
				{
					for (int z = 0; z < code->options_count; z++)
					{
						for (no = list_head(code->options[z].opts); (optval = list_get(no)); no = list_next(no))
						{
							if (optval->name)
								free(optval->name);
							if (optval->value)
								free(optval->value);

							free(optval);
						}
						list_free(code->options[z].opts);

						if (code->options[z].line)
							free(code->options[z].line);
					}
					
					free (code->options);
				}

				free(code);
			}
			
			list_free(item->codes);
			item->codes = NULL;
		}

		free(item);
	}

	list_free(list);
	
	LOG("UnloadGameList() :: Successfully unloaded game list");
}

int sortCodeList_Compare(const void* a, const void* b)
{
	return strcasecmp(((code_entry_t*) a)->name, ((code_entry_t*) b)->name);
}

/*
 * Function:		qsortSaveList_Compare()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Compares two game_entry for QuickSort
 * Arguments:
 *	a:				First code
 *	b:				Second code
 * Return:			1 if greater, -1 if less, or 0 if equal
 */
int sortSaveList_Compare(const void* a, const void* b)
{
	return strcasecmp(((save_entry_t*) a)->name, ((save_entry_t*) b)->name);
}

int sortSaveList_Compare_TitleID(const void* a, const void* b)
{
	char* ta = ((save_entry_t*) a)->title_id;
	char* tb = ((save_entry_t*) b)->title_id;

	if (!ta)
		return (-1);

	if (!tb)
		return (1);

	int ret = strcasecmp(ta, tb);

	return (ret ? ret : sortSaveList_Compare(a, b));
}

static int parseTypeFlags(int flags)
{
	if (flags & SAVE_FLAG_VMC)
		return FILE_TYPE_VMC;
	else if (flags & SAVE_FLAG_PS1)
		return FILE_TYPE_PS1;
	else if (flags & SAVE_FLAG_PSP)
		return FILE_TYPE_PSP;
	else if (flags & SAVE_FLAG_PSV)
		return FILE_TYPE_PSV;

	return 0;
}

int sortSaveList_Compare_Type(const void* a, const void* b)
{
	int ta = parseTypeFlags(((save_entry_t*) a)->flags);
	int tb = parseTypeFlags(((save_entry_t*) b)->flags);

	if (ta == tb)
		return sortSaveList_Compare(a, b);
	else if (ta < tb)
		return -1;
	else
		return 1;
}

static void read_usb_encrypted_saves(const char* userPath, list_t *list, uint64_t account)
{
	DIR *d, *d2;
	struct dirent *dir, *dir2;
	save_entry_t *item;
	char savePath[256];

	d = opendir(userPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (dir->d_stat.st_mode & SCE_S_IFDIR || strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
			continue;

		snprintf(savePath, sizeof(savePath), "%s%s", userPath, dir->d_name);
		d2 = opendir(savePath);

		if (!d2)
			continue;

		LOG("Reading %s...", savePath);

		while ((dir2 = readdir(d2)) != NULL)
		{
			if (!(dir2->d_stat.st_mode & SCE_S_IFREG) || endsWith(dir2->d_name, ".bin"))
				continue;

			snprintf(savePath, sizeof(savePath), "%s%s/%s.bin", userPath, dir->d_name, dir2->d_name);
			if (file_exists(savePath) != SUCCESS)
				continue;

			snprintf(savePath, sizeof(savePath), "(Encrypted) %s/%s", dir->d_name, dir2->d_name);
			item = _createSaveEntry(SAVE_FLAG_PSV | SAVE_FLAG_LOCKED, savePath);
			item->type = FILE_TYPE_PSV;

			asprintf(&item->path, "%s%s/", userPath, dir->d_name);
			asprintf(&item->title_id, "%.9s", dir->d_name);
			item->dir_name = strdup(dir2->d_name);

			if (apollo_config.account_id == account)
				item->flags |= SAVE_FLAG_OWNER;

			snprintf(savePath, sizeof(savePath), "%s%s/%s", userPath, dir->d_name, dir2->d_name);
			
			uint64_t size = 0;
			get_file_size(savePath, &size);
//			item->blocks = size / ORBIS_SAVE_DATA_BLOCK_SIZE;

			LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
			list_append(list, item);

		}
		closedir(d2);
	}

	closedir(d);
}

static void read_psp_savegames(const char* userPath, list_t *list, int flags)
{
	DIR *d;
	struct dirent *dir;
	save_entry_t *item;
	char sfoPath[256];

	d = opendir(userPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (!(dir->d_stat.st_mode & SCE_S_IFDIR) || strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
			continue;

		snprintf(sfoPath, sizeof(sfoPath), "%s%s/PARAM.SFO", userPath, dir->d_name);
		if (file_exists(sfoPath) != SUCCESS)
			continue;

		LOG("Reading %s...", sfoPath);
		sfo_context_t* sfo = sfo_alloc();
		if (sfo_read(sfo, sfoPath) < 0) {
			LOG("Unable to read from '%s'", sfoPath);
			sfo_free(sfo);
			continue;
		}

		snprintf(sfoPath, sizeof(sfoPath), "%s%s/SCEVMC0.VMP", userPath, dir->d_name);
		if ((strcmp((char*) sfo_get_param_value(sfo, "SAVEDATA_FILE_LIST"), "CONFIG.BIN") == 0) &&
			(file_exists(sfoPath) == SUCCESS))
		{
			item = _createSaveEntry(SAVE_FLAG_PS1 | flags, sfo_get_param_value(sfo, "TITLE"));
			item->type = FILE_TYPE_PSP;
			item->dir_name = strdup((char*) sfo_get_param_value(sfo, "SAVEDATA_DIRECTORY"));
			asprintf(&item->title_id, "%.9s", item->dir_name);
			asprintf(&item->path, "%s%s/", userPath, dir->d_name);
			list_append(list, item);

			snprintf(sfoPath, sizeof(sfoPath), "%s (MemCard)", sfo_get_param_value(sfo, "TITLE"));
			item = _createSaveEntry(SAVE_FLAG_PS1 | SAVE_FLAG_VMC | flags, sfoPath);
			item->type = FILE_TYPE_VMC;
			item->dir_name = strdup((char*) sfo_get_param_value(sfo, "SAVEDATA_DIRECTORY"));
			item->title_id = strdup("VMC 0");
			asprintf(&item->path, "%s%s/SCEVMC0.VMP", userPath, dir->d_name);
			list_append(list, item);

			item = _createSaveEntry(SAVE_FLAG_PS1 | SAVE_FLAG_VMC | flags, sfoPath);
			item->type = FILE_TYPE_VMC;
			item->dir_name = strdup((char*) sfo_get_param_value(sfo, "SAVEDATA_DIRECTORY"));
			item->title_id = strdup("VMC 1");
			asprintf(&item->path, "%s%s/SCEVMC1.VMP", userPath, dir->d_name);
		}
		else
		{
			item = _createSaveEntry(SAVE_FLAG_PSP | flags, (char*) sfo_get_param_value(sfo, "TITLE"));
			item->type = FILE_TYPE_PSP;
			item->dir_name = strdup((char*) sfo_get_param_value(sfo, "SAVEDATA_DIRECTORY"));
			asprintf(&item->title_id, "%.9s", item->dir_name);
			asprintf(&item->path, "%s%s/", userPath, dir->d_name);
		}

		sfo_free(sfo);
		LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}

	closedir(d);
}

static void read_usb_savegames(const char* userPath, list_t *list, sqlite3 *db)
{
	DIR *d;
	struct dirent *dir;
	save_entry_t *item;
	char sfoPath[256];

	d = opendir(userPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (!(dir->d_stat.st_mode & SCE_S_IFDIR) || strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
			continue;

		snprintf(sfoPath, sizeof(sfoPath), "%s%s/sce_sys/param.sfo", userPath, dir->d_name);
		if (file_exists(sfoPath) != SUCCESS)
			continue;

		LOG("Reading %s...", sfoPath);

		sfo_context_t* sfo = sfo_alloc();
		if (sfo_read(sfo, sfoPath) < 0) {
			LOG("Unable to read from '%s'", sfoPath);
			sfo_free(sfo);
			continue;
		}

		char *sfo_data = (char*)(sfo_get_param_value(sfo, "PARAMS") + 0x28);
		item = _createSaveEntry(SAVE_FLAG_PSV, get_appdb_title(db, sfo_data, sfoPath) ? sfoPath : sfo_data);
		item->type = FILE_TYPE_PSV;
		asprintf(&item->path, "%s%s/", userPath, dir->d_name);
		asprintf(&item->title_id, "%.9s", sfo_data);

		sfo_data = (char*)(sfo_get_param_value(sfo, "PARENT_DIRECTORY") + 1);
		item->dir_name = strdup(sfo_data);

		uint64_t* int_data = (uint64_t*) sfo_get_param_value(sfo, "ACCOUNT_ID");
		if (int_data && (apollo_config.account_id == *int_data))
			item->flags |= SAVE_FLAG_OWNER;

		sfo_free(sfo);
			
		LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}

	closedir(d);
}

static void read_hdd_savegames(const char* userPath, list_t *list)
{
	char sfoPath[256];
	save_entry_t *item;
	sqlite3_stmt *res;
	sqlite3 *db = open_sqlite_db(userPath);

	if (!db)
		return;

	int rc = sqlite3_prepare_v2(db, "SELECT a.titleId, val, title, iconPath FROM tbl_appinfo_icon AS a, tbl_appinfo AS b "
		" WHERE (type = 0) AND (a.titleId = b.titleId) AND (a.titleid NOT LIKE 'NPX%') AND (key = 278217076)", -1, &res, NULL);
	if (rc != SQLITE_OK)
	{
		LOG("Failed to fetch data: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	while (sqlite3_step(res) == SQLITE_ROW)
	{
		item = _createSaveEntry(SAVE_FLAG_PSV | SAVE_FLAG_HDD, (const char*) sqlite3_column_text(res, 2));
		item->type = FILE_TYPE_PSV;
		item->dir_name = strdup((const char*) sqlite3_column_text(res, 1));
		item->title_id = strdup((const char*) sqlite3_column_text(res, 0));
		item->blocks = 1; //sqlite3_column_int(res, 3);
		asprintf(&item->path, APOLLO_SANDBOX_PATH, item->dir_name);

		sfo_context_t* sfo = sfo_alloc();
		snprintf(sfoPath, sizeof(sfoPath), APOLLO_SANDBOX_PATH "sce_sys/param.sfo", item->dir_name);
		if (file_exists(sfoPath) == SUCCESS && sfo_read(sfo, sfoPath) == SUCCESS)
		{
			uint64_t* int_data = (uint64_t*) sfo_get_param_value(sfo, "ACCOUNT_ID");
			if (int_data && (apollo_config.account_id == *int_data))
				item->flags |= SAVE_FLAG_OWNER;
		}
		sfo_free(sfo);

		LOG("[%s] F(%X) {%d} '%s'", item->title_id, item->flags, item->blocks, item->name);
		list_append(list, item);
	}

	sqlite3_finalize(res);
	sqlite3_close(db);
}

/*
 * Function:		ReadUserList()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Reads the entire userlist folder into a game_entry array
 * Arguments:
 *	gmc:			Set as the number of games read
 * Return:			Pointer to array of game_entry, null if failed
 */
list_t * ReadUsbList(const char* userPath)
{
	save_entry_t *item;
	code_entry_t *cmd;
	list_t *list;
	sqlite3* appdb;

	if (dir_exists(userPath) != SUCCESS)
		return NULL;

	list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_PSV, CHAR_ICON_COPY " Bulk Save Management");
	item->type = FILE_TYPE_MENU;
	item->codes = list_alloc();
	item->path = strdup(userPath);
	//bulk management hack
	item->dir_name = malloc(sizeof(void**));
	((void**)item->dir_name)[0] = list;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Resign selected Saves", CMD_RESIGN_SAVES);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Resign all Saves", CMD_RESIGN_ALL_SAVES);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy selected Saves to User Storage (ux0:user/)", CMD_COPY_SAVES_HDD);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy all Saves to User Storage (ux0:user/)", CMD_COPY_ALL_SAVES_HDD);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_NET " Start local Web Server", CMD_SAVE_WEBSERVER);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Dump all Save Fingerprints", CMD_DUMP_FINGERPRINTS);
	list_append(item->codes, cmd);
	list_append(list, item);

	appdb = open_sqlite_db(USER_PATH_HDD);
	read_usb_savegames(userPath, list, appdb);
	read_psp_savegames(userPath, list, 0);
	read_vmc_files(userPath, list);
	sqlite3_close(appdb);

	return list;
}

list_t * ReadUserList(const char* userPath)
{
	save_entry_t *item;
	code_entry_t *cmd;
	list_t *list;

	if (file_exists(userPath) != SUCCESS)
		return NULL;

	list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_PSV, CHAR_ICON_COPY " Bulk Save Management");
	item->type = FILE_TYPE_MENU;
	item->codes = list_alloc();
	item->path = strdup(userPath);
	//bulk management hack
	item->dir_name = malloc(sizeof(void**));
	((void**)item->dir_name)[0] = list;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy selected Saves to Backup Storage", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(1, "Copy Saves to Backup Storage", CMD_COPY_SAVES_USB);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy all Saves to Backup Storage", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(1, "Copy Saves to Backup Storage", CMD_COPY_ALL_SAVES_USB);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_NET " Start local Web Server", CMD_SAVE_WEBSERVER);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Dump all Save Fingerprints", CMD_DUMP_FINGERPRINTS);
	list_append(item->codes, cmd);
	list_append(list, item);

	read_hdd_savegames(userPath, list);
	read_psp_savegames(PSP_SAVES_PATH_HDD, list, SAVE_FLAG_HDD);

	return list;
}

/*
 * Function:		ReadOnlineList()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Downloads the entire gamelist file into a game_entry array
 * Arguments:
 *	gmc:			Set as the number of games read
 * Return:			Pointer to array of game_entry, null if failed
 */
static void _ReadOnlineListEx(const char* urlPath, uint16_t flag, list_t *list)
{
	save_entry_t *item;
	char path[256];

	snprintf(path, sizeof(path), APOLLO_LOCAL_CACHE "%04X_games.txt", flag);

	if (file_exists(path) == SUCCESS && strcmp(apollo_config.save_db, ONLINE_URL) == 0)
	{
		struct stat stats;
		stat(path, &stats);
		// re-download if file is +1 day old
		if ((stats.st_mtime + ONLINE_CACHE_TIMEOUT) < time(NULL))
			http_download(urlPath, "games.txt", path, 0);
	}
	else
	{
		if (!http_download(urlPath, "games.txt", path, 0))
			return;
	}
	
	long fsize;
	char *data = readTextFile(path, &fsize);
	if (!data)
		return;
	
	char *ptr = data;
	char *end = data + fsize;

	while (ptr < end && *ptr)
	{
		char *tmp, *content = ptr;

		while (ptr < end && *ptr != '\n' && *ptr != '\r')
		{
			ptr++;
		}
		*ptr++ = 0;

		if ((tmp = strchr(content, '=')) != NULL)
		{
			*tmp++ = 0;
			item = _createSaveEntry(flag | SAVE_FLAG_ONLINE, tmp);
			item->title_id = strdup(content);
			asprintf(&item->path, "%s%s/", urlPath, item->title_id);

			LOG("+ [%s] %s", item->title_id, item->name);
			list_append(list, item);
		}

		if (ptr < end && *ptr == '\r')
		{
			ptr++;
		}
		if (ptr < end && *ptr == '\n')
		{
			ptr++;
		}
	}

	free(data);
}

list_t * ReadOnlineList(const char* urlPath)
{
	char url[256];
	list_t *list = list_alloc();

	// PSV save-games (Zip folder)
	snprintf(url, sizeof(url), "%s" "PSV/", urlPath);
	_ReadOnlineListEx(url, SAVE_FLAG_PSV, list);

	// PSP save-games (Zip folder)
	snprintf(url, sizeof(url), "%sPSP/", urlPath);
	_ReadOnlineListEx(url, SAVE_FLAG_PSP, list);

	// PS1 save-games (Zip PSV)
	snprintf(url, sizeof(url), "%sPS1/", urlPath);
	_ReadOnlineListEx(url, SAVE_FLAG_PS1, list);

	if (!list_count(list))
	{
		list_free(list);
		return NULL;
	}

	return list;
}

list_t * ReadVmcList(const char* userPath)
{
	char filePath[256];
	save_entry_t *item;
	code_entry_t *cmd;
	list_t *list;
	ps1mcData_t* mcdata;

	if (!openMemoryCard(userPath, 0))
	{
		LOG("Error: no PS1 Memory Card detected! (%s)", userPath);
		return NULL;
	}

	mcdata = getMemoryCardData();
	if (!mcdata)
		return NULL;

	list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_PS1, CHAR_ICON_VMC " Memory Card Management");
	item->type = FILE_TYPE_MENU;
	item->path = strdup(userPath);
	item->codes = list_alloc();
	//bulk management hack
	item->dir_name = malloc(sizeof(void**));
	((void**)item->dir_name)[0] = list;

	strncpy(filePath, userPath, sizeof(filePath));
	strrchr(filePath, '/')[0] = 0;
	item->title_id = strdup(strrchr(filePath, '/')+1);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export selected Saves to Backup Storage", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(1, "Copy Saves to Backup Storage", CMD_EXP_SAVES_VMC);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export all Saves to Backup Storage", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(1, "Copy Saves to Backup Storage", CMD_EXP_ALL_SAVES_VMC);
	list_append(item->codes, cmd);
	add_vmp_commands(item);
	list_append(list, item);

	item = _createSaveEntry(SAVE_FLAG_PS1, CHAR_ICON_COPY " Import Saves to Virtual Card");
	asprintf(&item->path, "%s:/data/", USER_STORAGE_DEV);
	asprintf(&item->title_id, " %s:/", USER_STORAGE_DEV);
	item->dir_name = strdup(userPath);
	item->type = FILE_TYPE_MENU;
	list_append(list, item);

	for (int i = 0; i < PS1CARD_MAX_SLOTS; i++)
	{
		if (mcdata[i].saveType != PS1BLOCK_INITIAL)
			continue;

		LOG("Reading '%s'...", mcdata[i].saveName);

		char* tmp = sjis2utf8(mcdata[i].saveTitle);
		item = _createSaveEntry(SAVE_FLAG_PS1 | SAVE_FLAG_VMC, tmp);
		item->blocks = i;
		item->type = FILE_TYPE_PS1;
		item->dir_name = strdup(mcdata[i].saveName);
		item->title_id = strdup(mcdata[i].saveProdCode);
		asprintf(&item->path, "%s\n%s/", userPath, mcdata[i].saveName);
		free(tmp);

		LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}

	return list;
}

static int sqlite_trophy_collate(void *foo, int ll, const void *l, int rl, const void *r)
{
    return 0;
}

list_t * ReadTrophyList(const char* userPath)
{
	char filePath[256];
	save_entry_t *item;
	code_entry_t *cmd;
	list_t *list;
	sqlite3 *db;
	sqlite3_stmt *res;
	const char* dev[] = {UMA0_PATH, IMC0_PATH, UX0_PATH};

	if ((db = open_sqlite_db(userPath)) == NULL)
		return NULL;

	list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_PSV, CHAR_ICON_COPY " Bulk Trophy Management");
	item->type = FILE_TYPE_MENU;
	item->path = strdup(userPath);
	item->codes = list_alloc();
	//bulk management hack
	item->dir_name = malloc(sizeof(void**));
	((void**)item->dir_name)[0] = list;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Backup selected Trophies to Backup Storage", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(1, "Copy Trophies to Backup Storage", CMD_COPY_TROPHIES_USB);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Backup all Trophies to Backup Storage", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(1, "Copy Trophies to Backup Storage", CMD_COPY_ALL_TROP_USB);
	list_append(item->codes, cmd);
	list_append(list, item);

	for (int i = 0; i < 3; i++)
	{
		snprintf(filePath, sizeof(filePath), "%s" TROPHIES_PATH_USB, dev[i]);
		if (i && dir_exists(filePath) != SUCCESS)
			continue;

		item = _createSaveEntry(SAVE_FLAG_PSV | SAVE_FLAG_TROPHY, CHAR_ICON_COPY " Import Trophies");
		asprintf(&item->path, "%s" TROPHIES_PATH_USB, dev[i]);
		asprintf(&item->title_id, " %s", dev[i]);
		item->type = FILE_TYPE_MENU;
		list_append(list, item);
	}

	sqlite3_create_collation(db, "trophy_collator", SQLITE_UTF8, NULL, &sqlite_trophy_collate);
	int rc = sqlite3_prepare_v2(db, "SELECT id, npcommid, title FROM tbl_trophy_title WHERE status = 0", -1, &res, NULL);
	if (rc != SQLITE_OK)
	{
		LOG("Failed to fetch data: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return NULL;
	}

	while (sqlite3_step(res) == SQLITE_ROW)
	{
		item = _createSaveEntry(SAVE_FLAG_PSV | SAVE_FLAG_TROPHY | SAVE_FLAG_HDD, (const char*) sqlite3_column_text(res, 2));
		item->blocks = sqlite3_column_int(res, 0);
		item->title_id = strdup((const char*) sqlite3_column_text(res, 1));
		asprintf(&item->path, TROPHY_PATH_HDD "data/%s/", apollo_config.user_id, item->title_id);
		item->type = FILE_TYPE_TRP;

		LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}

	sqlite3_finalize(res);
	sqlite3_close(db);

	return list;
}

int get_save_details(const save_entry_t* save, char **details)
{
	char sfoPath[256];
	sqlite3 *db;
	sqlite3_stmt *res;
	sdslot_dat_t* sdslot;
	size_t size;

	if (save->type == FILE_TYPE_PS1)
	{
		asprintf(details, "%s\n----- PS1 Save -----\n"
			"Game: %s\n"
			"Title ID: %s\n"
			"Folder: %s\n",
			save->path,
			save->name,
			save->title_id,
			save->dir_name);
		return 1;
	}

	if (save->type == FILE_TYPE_VMC)
	{
		asprintf(details, "%s\n----- Virtual Memory Card -----\n"
			"Game: %s\n"
			"Type: %s\n"
			"Folder: %s\n",
			save->path,
			save->name,
			save->title_id,
			save->dir_name);
		return 1;
	}

	if (save->flags & SAVE_FLAG_ONLINE)
	{
		asprintf(details, "%s\n----- Online Database -----\n"
			"Game: %s\n"
			"Title ID: %s\n",
			save->path,
			save->name,
			save->title_id);
		return 1;
	}

	if (save->flags & SAVE_FLAG_PSP || save->flags & SAVE_FLAG_PS1)
	{
		snprintf(sfoPath, sizeof(sfoPath), "%sPARAM.SFO", save->path);
		LOG("Save Details :: Reading %s...", sfoPath);

		sfo_context_t* sfo = sfo_alloc();
		if (sfo_read(sfo, sfoPath) < 0) {
			LOG("Unable to read from '%s'", sfoPath);
			sfo_free(sfo);
			return 0;
		}

		asprintf(details, "%s\n----- PSP Save -----\n"
			"Game: %s\n"
			"Title ID: %s\n"
			"Folder: %s\n"
			"Title: %s\n"
			"Details: %s\n",
			save->path,
			save->name,
			save->title_id,
			save->dir_name,
			(char*)sfo_get_param_value(sfo, "SAVEDATA_TITLE"),
			(char*)sfo_get_param_value(sfo, "SAVEDATA_DETAIL"));

		sfo_free(sfo);
		return 1;
	}

	if (!(save->flags & SAVE_FLAG_PSV))
	{
		asprintf(details, "%s\n\nTitle: %s\n", save->path, save->name);
		return 1;
	}

	if (save->flags & SAVE_FLAG_TROPHY)
	{
		snprintf(sfoPath, sizeof(sfoPath), TROPHY_PATH_HDD "db/trophy_local.db", apollo_config.user_id);
		if ((db = open_sqlite_db(sfoPath)) == NULL)
			return 0;

		char* query = sqlite3_mprintf("SELECT id, description, trophy_num, unlocked_trophy_num, progress,"
			"platinum_num, unlocked_platinum_num, gold_num, unlocked_gold_num, silver_num, unlocked_silver_num,"
			"bronze_num, unlocked_bronze_num FROM tbl_trophy_title WHERE id = %d", save->blocks);

		if (sqlite3_prepare_v2(db, query, -1, &res, NULL) != SQLITE_OK || sqlite3_step(res) != SQLITE_ROW)
		{
			LOG("Failed to fetch data: %s", sqlite3_errmsg(db));
			sqlite3_free(query);
			sqlite3_close(db);
			return 0;
		}

		asprintf(details, "Trophy-Set Details\n\n"
			"Title: %s\n"
			"Description: %s\n"
			"NP Comm ID: %s\n"
			"Progress: %d/%d - %d%%\n"
			"%c Platinum: %d/%d\n"
			"%c Gold: %d/%d\n"
			"%c Silver: %d/%d\n"
			"%c Bronze: %d/%d\n",
			save->name, sqlite3_column_text(res, 1), save->title_id,
			sqlite3_column_int(res, 3), sqlite3_column_int(res, 2), sqlite3_column_int(res, 4),
			CHAR_TRP_PLATINUM, sqlite3_column_int(res, 6), sqlite3_column_int(res, 5),
			CHAR_TRP_GOLD, sqlite3_column_int(res, 8), sqlite3_column_int(res, 7),
			CHAR_TRP_SILVER, sqlite3_column_int(res, 10), sqlite3_column_int(res, 9),
			CHAR_TRP_BRONZE, sqlite3_column_int(res, 12), sqlite3_column_int(res, 11));

		sqlite3_free(query);
		sqlite3_finalize(res);
		sqlite3_close(db);

		return 1;
	}

	if(save->flags & SAVE_FLAG_LOCKED)
	{
		asprintf(details, "%s\n\n"
			"Title ID: %s\n"
			"Dir Name: %s\n"
			"Blocks: %d\n"
			"Account ID: %.16s\n",
			save->path,
			save->title_id,
			save->dir_name,
			save->blocks,
			save->path + 23);

		return 1;
	}

	if(save->flags & SAVE_FLAG_HDD)
		vita_SaveMount(save);

	snprintf(sfoPath, sizeof(sfoPath), "%ssce_sys/param.sfo", save->path);
	LOG("Save Details :: Reading %s...", sfoPath);

	sfo_context_t* sfo = sfo_alloc();
	if (sfo_read(sfo, sfoPath) < 0) {
		LOG("Unable to read from '%s'", sfoPath);
		sfo_free(sfo);
		return 0;
	}

	strcpy(strrchr(sfoPath, '/'), "/sdslot.dat");
	LOG("Save Details :: Reading %s...", sfoPath);
	if (read_buffer(sfoPath, (uint8_t**) &sdslot, &size) != SUCCESS) {
		LOG("Unable to read from '%s'", sfoPath);
		sfo_free(sfo);
		return 0;
	}

	if (sdslot->header.magic == 0x4C534453)
		memcpy(sfoPath, sdslot->header.active_slots, sizeof(sfoPath));
	else
		memset(sfoPath, 0, sizeof(sfoPath));

	char* out = *details = (char*) sdslot;
	uint64_t* account_id = (uint64_t*) sfo_get_param_value(sfo, "ACCOUNT_ID");

	out += sprintf(out, "%s\n----- Save -----\n"
		"Title: %s\n"
		"Title ID: %s\n"
		"Dir Name: %s\n"
		"Account ID: %016llx\n",
		save->path, save->name,
		save->title_id,
		save->dir_name,
		*account_id);

	for (int i = 0; (i < 256) && sfoPath[i]; i++)
	{
		out += sprintf(out, "----- Slot %03d -----\n"
			"Date: %d/%02d/%02d %02d:%02d:%02d\n"
			"Title: %s\n"
			"Subtitle: %s\n"
			"Details: %s\n",
			(i+1), sdslot->slots[i].year, sdslot->slots[i].month, sdslot->slots[i].day,
			sdslot->slots[i].hour, sdslot->slots[i].min, sdslot->slots[i].sec,
			sdslot->slots[i].title,
			sdslot->slots[i].subtitle,
			sdslot->slots[i].description);
	}

	if(save->flags & SAVE_FLAG_HDD)
		vita_SaveUmount();

	sfo_free(sfo);
	return 1;
}
