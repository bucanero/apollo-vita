#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <psp2/registrymgr.h>
#include <psp2/vshbridge.h>

#include "types.h"
#include "menu.h"
#include "saves.h"
#include "common.h"


static char* ext_src[] = {"ux0", "uma0", "imc0", "xmc0", "ur0", NULL};
static char* sort_opt[] = {"Disabled", "by Name", "by Title ID", NULL};

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

void sort_callback(int sel)
{
	apollo_config.doSort = sel;
}

void ani_callback(int sel)
{
	apollo_config.doAni = !sel;
}

void clearcache_callback(int sel)
{
	LOG("Cleaning folder '%s'...", APOLLO_LOCAL_CACHE);
	clean_directory(APOLLO_LOCAL_CACHE);

	show_message("Local cache folder cleaned:\n" APOLLO_LOCAL_CACHE);
}

void unzip_app_data(const char* zip_file)
{
	if (extract_zip(zip_file, APOLLO_DATA_PATH))
		show_message("Successfully installed local application data");

	unlink_secure(zip_file);
}

void upd_appdata_callback(int sel)
{
	if (http_download(ONLINE_URL, "PSV/psvappdata.zip", APOLLO_LOCAL_CACHE "appdata.zip", 1))
		unzip_app_data(APOLLO_LOCAL_CACHE "appdata.zip");
}

void update_callback(int sel)
{
    apollo_config.update = !sel;

    if (!apollo_config.update)
        return;

	LOG("checking latest Apollo version at %s", APOLLO_UPDATE_URL);

	if (!http_download(APOLLO_UPDATE_URL, "", APOLLO_LOCAL_CACHE "ver.check", 0))
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

	if (show_dialog(1, "New version available! Download update?"))
	{
		if (http_download(start, "", "ux0:data/apollo-vita.vpk", 1))
			show_message("Update downloaded to ux0:data/apollo-vita.vpk");
		else
			show_message("Download error!");
	}

end_update:
	free(buffer);
	return;
}

void owner_callback(int sel)
{
	apollo_config.storage = sel;
}

void log_callback(int sel)
{
	dbglogger_init_mode(FILE_LOGGER, APOLLO_PATH "apollo.log", 0);
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
		file_data->idps[0] = config->idps[0];
		file_data->idps[1] = config->idps[1];
		memcpy(config, file_data, file_size);

		LOG("Settings loaded: UserID (%08x) AccountID (%016llX)", config->user_id, config->account_id);
		free(file_data);
	}

	vita_SaveUmount();
	return 1;
}
