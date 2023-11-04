#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <psp2/registrymgr.h>
#include <psp2/vshbridge.h>
#include <psp2/kernel/openpsid.h>
#include <libxml/parser.h>

#include "types.h"
#include "menu.h"
#include "saves.h"
#include "common.h"
#include "plugin.h"

#define GAME_PLUGIN_PATH             "ux0:pspemu/seplugins/game.txt"
#define SGKEY_DUMP_PLUGIN_PATH       "ux0:pspemu/seplugins/SGKeyDumper.prx"

char *strcasestr(const char *, const char *);
static char* ext_src[MAX_USB_DEVICES+1] = {"ux0", "uma0", "imc0", "xmc0", "ur0", NULL};
static char* sort_opt[] = {"Disabled", "by Name", "by Title ID", NULL};

static void log_callback(int sel);
static void sort_callback(int sel);
static void ani_callback(int sel);
static void owner_callback(int sel);
static void db_url_callback(int sel);
static void clearcache_callback(int sel);
static void upd_appdata_callback(int sel);

menu_option_t menu_options[] = {
	{ .name = "\nBackground Music", 
		.options = NULL, 
		.type = APP_OPTION_BOOL, 
		.value = &apollo_config.music, 
		.callback = music_callback 
	},
	{ .name = "Menu Animations", 
		.options = NULL, 
		.type = APP_OPTION_BOOL, 
		.value = &apollo_config.doAni, 
		.callback = ani_callback 
	},
	{ .name = "Sort Saves", 
		.options = (char**) sort_opt,
		.type = APP_OPTION_LIST,
		.value = &apollo_config.doSort, 
		.callback = sort_callback 
	},
	{ .name = "\nExternal Saves Source",
		.options = (char**) ext_src,
		.type = APP_OPTION_LIST,
		.value = &apollo_config.storage,
		.callback = owner_callback
	},
	{ .name = "Version Update Check", 
		.options = NULL, 
		.type = APP_OPTION_BOOL, 
		.value = &apollo_config.update, 
		.callback = update_callback 
	},
	{ .name = "Change Online Database URL",
		.options = NULL,
		.type = APP_OPTION_CALL,
		.value = NULL,
		.callback = db_url_callback 
	},
	{ .name = "\nUpdate Application Data", 
		.options = NULL, 
		.type = APP_OPTION_CALL, 
		.value = NULL, 
		.callback = upd_appdata_callback 
	},
	{ .name = "Clear Local Cache", 
		.options = NULL, 
		.type = APP_OPTION_CALL, 
		.value = NULL, 
		.callback = clearcache_callback 
	},
	{ .name = "\nEnable Debug Log",
		.options = NULL,
		.type = APP_OPTION_CALL,
		.value = NULL,
		.callback = log_callback 
	},
	{ .name = NULL }
};


void music_callback(int sel)
{
	apollo_config.music = !sel;
}

static void sort_callback(int sel)
{
	apollo_config.doSort = sel;
}

static void ani_callback(int sel)
{
	apollo_config.doAni = !sel;
}

static void db_url_callback(int sel)
{
	if (osk_dialog_get_text("Enter the URL of the online database", apollo_config.save_db, sizeof(apollo_config.save_db)))
		show_message("Online database URL changed to:\n%s", apollo_config.save_db);

	if (apollo_config.save_db[strlen(apollo_config.save_db)-1] != '/')
		strcat(apollo_config.save_db, "/");
}

static void clearcache_callback(int sel)
{
	LOG("Cleaning folder '%s'...", APOLLO_LOCAL_CACHE);
	clean_directory(APOLLO_LOCAL_CACHE);

	show_message("Local cache folder cleaned:\n" APOLLO_LOCAL_CACHE);
}

static void upd_appdata_callback(int sel)
{
	int i;

	if (!http_download(ONLINE_PATCH_URL, "apollo-vita-update.zip", APOLLO_LOCAL_CACHE "appdata.zip", 1))
		show_message("Error! Can't download data update pack!");

	if ((i = extract_zip(APOLLO_LOCAL_CACHE "appdata.zip", APOLLO_DATA_PATH)) > 0)
		show_message("Successfully updated %d data files!", i);
	else
		show_message("Error! Can't extract data update pack!");

	unlink_secure(APOLLO_LOCAL_CACHE "appdata.zip");
}

void update_callback(int sel)
{
    apollo_config.update = !sel;

    if (!apollo_config.update)
        return;

	LOG("checking latest Apollo version at %s", APOLLO_UPDATE_URL);

	if (!http_download(APOLLO_UPDATE_URL, NULL, APOLLO_LOCAL_CACHE "ver.check", 0))
	{
		LOG("http request to %s failed", APOLLO_UPDATE_URL);
		return;
	}

	char *buffer;
	long size = 0;

	buffer = readTextFile(APOLLO_LOCAL_CACHE "ver.check", &size);

	if (!buffer)
		return;

	LOG("received %ld bytes", size);

	static const char find[] = "\"name\":\"Apollo Save Tool v";
	const char* start = strstr(buffer, find);
	if (!start)
	{
		LOG("no name found");
		goto end_update;
	}

	LOG("found name");
	start += sizeof(find) - 1;

	char* end = strchr(start, '"');
	if (!end)
	{
		LOG("no end of name found");
		goto end_update;
	}
	*end = 0;
	LOG("latest version is %s", start);

	if (strcasecmp(APOLLO_VERSION, start) == 0)
	{
		LOG("no need to update");
		goto end_update;
	}

	start = strstr(end+1, "\"browser_download_url\":\"");
	if (!start)
		goto end_update;

	start += 24;
	end = strchr(start, '"');
	if (!end)
	{
		LOG("no download URL found");
		goto end_update;
	}

	*end = 0;
	LOG("download URL is %s", start);

	if (show_dialog(DIALOG_TYPE_YESNO, "New version available! Download update?"))
	{
		if (http_download(start, NULL, "ux0:data/apollo-vita.vpk", 1))
			show_message("Update downloaded to ux0:data/apollo-vita.vpk");
		else
			show_message("Download error!");
	}

end_update:
	free(buffer);
	return;
}

static void owner_callback(int sel)
{
	apollo_config.storage = sel;
}

static void log_callback(int sel)
{
	dbglogger_init_mode(FILE_LOGGER, APOLLO_PATH "apollo.log", 1);
	show_message("Debug Logging Enabled!\n\n" APOLLO_PATH "apollo.log");
}

int save_app_settings(app_config_t* config)
{
	char filePath[256];
	char title[32] = "NP0APOLLO";
	save_entry_t se = {
		.dir_name = title,
		.title_id = title,
		.path = filePath,
	};

	LOG("Apollo Save Tool v%s - Patch Engine v%s", APOLLO_VERSION, APOLLO_LIB_VERSION);
	snprintf(filePath, sizeof(filePath), APOLLO_SANDBOX_PATH, title);
	if (!vita_SaveMount(&se)) {
		LOG("sceSaveDataMount2 ERROR");
		return 0;
	}

	LOG("Saving Settings...");
	snprintf(filePath, sizeof(filePath), APOLLO_SANDBOX_PATH "settings.bin", title);
	write_buffer(filePath, (uint8_t*) config, sizeof(app_config_t));

	vita_SaveUmount();
	return 1;
}

int load_app_settings(app_config_t* config)
{
	char filePath[256];
	app_config_t* file_data;
	size_t file_size;
	char title[32] = "NP0APOLLO";
	save_entry_t se = {
		.dir_name = title,
		.title_id = title,
		.path = filePath,
	};

	config->user_id = 0;
	if (sceRegMgrGetKeyBin("/CONFIG/NP", "account_id", &config->account_id, sizeof(uint64_t)) < 0)
	{
		LOG("Failed to get account_id");
		config->account_id = 0;
	}

	sceKernelGetOpenPsId((SceKernelOpenPsId*) config->psid);
	_vshSblAimgrGetConsoleId((char*) config->idps);
	config->idps[0] = ES64(config->idps[0]);
	config->idps[1] = ES64(config->idps[1]);

	snprintf(filePath, sizeof(filePath), APOLLO_SANDBOX_PATH, title);
	if (vita_SaveMount(&se) < 0) {
		LOG("sceSaveDataMount2 ERROR");
		return 0;
	}

	LOG("Loading Settings...");
	snprintf(filePath, sizeof(filePath), APOLLO_SANDBOX_PATH "settings.bin", title);

	if (read_buffer(filePath, (uint8_t**) &file_data, &file_size) == SUCCESS && file_size == sizeof(app_config_t))
	{
		file_data->user_id = config->user_id;
		file_data->account_id = config->account_id;
		memcpy(file_data->idps, config->idps, sizeof(uint64_t)*2);
		memcpy(file_data->psid, config->psid, sizeof(uint64_t)*2);
		memcpy(config, file_data, file_size);

		LOG("Settings loaded: UserID (%08x) AccountID (%016llX)", config->user_id, config->account_id);
		free(file_data);
	}

	vita_SaveUmount();
	return 1;
}

static xmlNode* _get_owner_node(xmlNode *a_node, xmlChar *owner_name)
{
    xmlNode *cur_node = NULL;

    for (cur_node = a_node; cur_node; cur_node = cur_node->next)
    {
        if (cur_node->type != XML_ELEMENT_NODE)
            continue;

        if ((xmlStrcasecmp(cur_node->name, BAD_CAST "owner") == 0) &&
            (xmlStrcmp(xmlGetProp(cur_node, BAD_CAST "name"), owner_name) == 0))
            return cur_node;
    }

    return NULL;
}

int save_xml_owner(const char *xmlfile)
{
    xmlDocPtr doc = NULL;       /* document pointer */
    xmlNodePtr root_node = NULL, node = NULL, node1 = NULL;/* node pointers */
    char buff[64];

    /*parse the file and get the DOM */
    doc = xmlParseFile(xmlfile);
    if (doc)
    {
        LOG("XML: updating file '%s'", xmlfile);
        /*Get the root element node */
        root_node = xmlDocGetRootElement(doc);
        node = _get_owner_node(root_node->children, BAD_CAST menu_about_strings_project[1]);

        if (node)
        {
            xmlUnlinkNode(node);
            xmlFreeNode(node);
        }
    }
    else
    {
        LOG("XML: could not parse data, creating new file '%s'", xmlfile);
        /* Creates a new document, a node and set it as a root node */
        doc = xmlNewDoc(BAD_CAST "1.0");
        root_node = xmlNewNode(NULL, BAD_CAST "apollo");
        xmlNewProp(root_node, BAD_CAST "platform", BAD_CAST "vita");
        xmlNewProp(root_node, BAD_CAST "version", BAD_CAST APOLLO_VERSION);
        xmlDocSetRootElement(doc, root_node);
    }

    /* 
     * xmlNewChild() creates a new node, which is "attached" as child node
     * of root_node node. 
     */
    node = xmlNewChild(root_node, NULL, BAD_CAST "owner", NULL);
    xmlNewProp(node, BAD_CAST "name", BAD_CAST menu_about_strings_project[1]);

    node1 = xmlNewChild(node, NULL, BAD_CAST "console", NULL);

    snprintf(buff, sizeof(buff), "%016llX %016llX", apollo_config.idps[0], apollo_config.idps[1]);
    xmlNewProp(node1, BAD_CAST "idps", BAD_CAST buff);

    snprintf(buff, sizeof(buff), "%016X %016X", 0, 0);
    xmlNewProp(node1, BAD_CAST "psid", BAD_CAST buff);

    node1 = xmlNewChild(node, NULL, BAD_CAST "user", NULL);

    snprintf(buff, sizeof(buff), "%08d", apollo_config.user_id);
    xmlNewProp(node1, BAD_CAST "id", BAD_CAST buff);

    snprintf(buff, sizeof(buff), "%016llx", apollo_config.account_id);
    xmlNewProp(node1, BAD_CAST "account_id", BAD_CAST buff);

    /* Dumping document to file */
    xmlSaveFormatFileEnc(xmlfile, doc, "UTF-8", 1);

    /*free the document */
    xmlFreeDoc(doc);
    xmlCleanupParser();

    return(1);
}

int install_sgkey_plugin(int install)
{
	char* data;
	size_t size;

	mkdirs(SGKEY_DUMP_PLUGIN_PATH);
	if (write_buffer(SGKEY_DUMP_PLUGIN_PATH, sgk_plugin, size_sgk_plugin) < 0)
		return 0;

	if (read_buffer(GAME_PLUGIN_PATH, (uint8_t**) &data, &size) < 0)
	{
		LOG("Error reading game.txt");
		if (!install)
			return 0;

		if (write_buffer(GAME_PLUGIN_PATH, SGKEY_DUMP_PLUGIN_PATH " 1\n", 39) < 0)
		{
			LOG("Error creating game.txt");
			return 0;
		}

		return 1;
	}

	if (install)
	{
		char *ptr = strcasestr(data, SGKEY_DUMP_PLUGIN_PATH " ");
		if (ptr != NULL && (ptr[37] == '1' || ptr[37] == '0'))
		{
			LOG("Plugin enabled");
			ptr[37] = '1';
			write_buffer(GAME_PLUGIN_PATH, data, size);
			free(data);

			return 1;
		}
		free(data);

		FILE* fp = fopen(GAME_PLUGIN_PATH, "a");
		if (!fp)
		{
			LOG("Error opening game.txt");
			return 0;
		}

		fprintf(fp, "%s%s", SGKEY_DUMP_PLUGIN_PATH, " 1\n");
		fclose(fp);
		return 1;
	}

	if (!install)
	{
		char *ptr = strcasestr(data, SGKEY_DUMP_PLUGIN_PATH " ");
		if (ptr != NULL && (ptr[37] == '1' || ptr[37] == '0'))
		{
			LOG("Plugin disabled");
			ptr[37] = '0';
			write_buffer(GAME_PLUGIN_PATH, data, size);
		}
		free(data);
		return 1;
	}

	return 0;
}
