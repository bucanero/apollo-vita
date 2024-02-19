/* 
	Apollo PS Vita main.c
*/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <taihen.h>
#include <psp2/ctrl.h>
#include <psp2/appmgr.h>
#include <psp2/apputil.h>
#include <psp2/system_param.h>
#include <psp2/sysmodule.h>
#include <psp2/vshbridge.h>
#include <psp2/audioout.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/modulemgr.h>
#include <libxmp-lite/xmp.h>

#include "saves.h"
#include "sfo.h"
#include "utils.h"
#include "common.h"
#include "vitapad.h"

//Font
#include "libfont.h"
#include "ttf_render.h"
#include "font_adonais.h"
#include "font-10x20.h"

//Menus
#include "menu.h"
#include "menu_gui.h"

//Sound
#define SAMPLING_FREQ       48000 /* 48khz. */
#define AUDIO_SAMPLES       256   /* audio samples */

// Audio handle
static int32_t audio = 0;
extern const uint8_t _binary_data_haiku_s3m_start;
extern const uint8_t _binary_data_haiku_s3m_size;


#define load_menu_texture(name, type) \
	({ extern const uint8_t _binary_data_##name##_##type##_start; \
		extern const uint8_t _binary_data_##name##_##type##_size; \
		menu_textures[name##_##type##_index].buffer = &_binary_data_##name##_##type##_start; \
		menu_textures[name##_##type##_index].size = (int) &_binary_data_##name##_##type##_size; \
		menu_textures[name##_##type##_index].texture = NULL; \
	}); \
		if (!LoadMenuTexture(NULL, name##_##type##_index)) return 0;

void update_usb_path(char *p);
void update_hdd_path(char *p);
void update_trophy_path(char *p);
void update_db_path(char *p);
void update_vmc_path(char *p);

app_config_t apollo_config = {
    .app_name = "APOLLO",
    .app_ver = APOLLO_VERSION,
    .save_db = ONLINE_URL,
    .music = 1,
    .doSort = 1,
    .doAni = 1,
    .update = 1,
    .storage = 0,
    .user_id = 0,
    .idps = {0, 0},
    .psid = {0, 0},
    .account_id = 0,
};

int close_app = 0;
int idle_time = 0;                          // Set by readPad

png_texture * menu_textures;                // png_texture array for main menu, initialized in LoadTexture
SDL_Window* window;                         // SDL window
SDL_Renderer* renderer;                     // SDL software renderer
uint32_t* texture_mem;                      // Pointers to texture memory
uint32_t* free_mem;                         // Pointer after last texture


char user_id_str[SCE_SYSTEM_PARAM_USERNAME_MAXSIZE] = "Apollo";
char psid_str[SFO_PSID_SIZE*2+2] = "0000000000000000 0000000000000000";
char account_id_str[SFO_ACCOUNT_ID_SIZE*2+1] = "0000000000000000";

const char * menu_about_strings_project[] = { "User ID", user_id_str,
											"Account ID", account_id_str,
											"Console IDPS", psid_str,
											NULL };

const char * menu_pad_help[TOTAL_MENU_IDS] = { NULL,												//Main
								"\x10 Select    \x13 Back    \x12 Details    \x11 Refresh",			//Trophy list
								"\x10 Select    \x13 Back    \x12 Details    \x11 Refresh",			//USB list
								"\x10 Select    \x13 Back    \x12 Details    \x11 Refresh",			//HDD list
								"\x10 Select    \x13 Back    \x11 Refresh",							//Online list
								"\x10 Select    \x13 Back    \x11 Refresh",							//User backup
								"\x10 Select    \x13 Back",											//Options
								"\x13 Back",														//About
								"\x10 Select    \x12 View Code    \x13 Back",						//Select Cheats
								"\x13 Back",														//View Cheat
								"\x10 Select    \x13 Back",											//Cheat Option
								"\x13 Back",														//View Details
								"\x10 Value Up  \x11 Value Down   \x13 Exit",						//Hex Editor
								};

/*
* HDD save list
*/
save_list_t hdd_saves = {
    .icon_id = cat_hdd_png_index,
    .title = "Internal Saves",
    .list = NULL,
    .path = "",
    .ReadList = &ReadUserList,
    .ReadCodes = &ReadCodes,
    .UpdatePath = &update_hdd_path,
};

/*
* USB save list
*/
save_list_t usb_saves = {
    .icon_id = cat_usb_png_index,
    .title = "External Saves",
    .list = NULL,
    .path = "",
    .ReadList = &ReadUsbList,
    .ReadCodes = &ReadCodes,
    .UpdatePath = &update_usb_path,
};

/*
* Trophy list
*/
save_list_t trophies = {
	.icon_id = cat_warning_png_index,
	.title = "Trophies",
    .list = NULL,
    .path = "",
    .ReadList = &ReadTrophyList,
    .ReadCodes = &ReadTrophies,
    .UpdatePath = &update_trophy_path,
};

/*
* Online code list
*/
save_list_t online_saves = {
	.icon_id = cat_db_png_index,
	.title = "Online Database",
    .list = NULL,
    .path = ONLINE_URL,
    .ReadList = &ReadOnlineList,
    .ReadCodes = &ReadOnlineSaves,
    .UpdatePath = &update_db_path,
};

/*
* User Backup code list
*/
save_list_t user_backup = {
    .icon_id = cat_bup_png_index,
    .title = "User Tools",
    .list = NULL,
    .path = "",
    .ReadList = &ReadBackupList,
    .ReadCodes = &ReadBackupCodes,
    .UpdatePath = NULL,
};

/*
* PS1 VMC list
*/
save_list_t vmc_saves = {
    .icon_id = cat_usb_png_index,
    .title = "Virtual Memory Card",
    .list = NULL,
    .path = "",
    .ReadList = &ReadVmcList,
    .ReadCodes = &ReadVmcCodes,
    .UpdatePath = &update_vmc_path,
};


static int initPad(void)
{
    int val;

    // Set enter button
    if (sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_ENTER_BUTTON, &val) < 0)
    {
        LOG("sceAppUtilSystemParamGetInt Error");
        val = 1;
    }

    if (vitaPadInit(val) < 0)
    {
        LOG("[ERROR] Failed to open pad!");
        return 0;
    }

    return 1;
}

// Used only in initialization. Allocates 64 mb for textures and loads the font
static int LoadTextures_Menu(void)
{
	texture_mem = malloc(256 * 8 * 4);
	menu_textures = (png_texture *)calloc(TOTAL_MENU_TEXTURES, sizeof(png_texture));
	
	if(!texture_mem || !menu_textures)
		return 0; // fail!
	
	ResetFont();
	free_mem = (u32 *) AddFontFromBitmapArray((u8 *) data_font_Adonais, (u8 *) texture_mem, 0x20, 0x7e, 32, 31, 1, BIT7_FIRST_PIXEL);
	free_mem = (u32 *) AddFontFromBitmapArray((u8 *) console_font_10x20, (u8 *) free_mem, 0, 0xFF, 10, 20, 1, BIT7_FIRST_PIXEL);

	if (TTFLoadFont(0, "sa0:data/font/pvf/ltn0.pvf", NULL, 0) != SUCCESS ||
		TTFLoadFont(1, "sa0:data/font/pvf/jpn0.pvf", NULL, 0) != SUCCESS ||
		TTFLoadFont(2, "sa0:data/font/pvf/kr0.pvf", NULL, 0) != SUCCESS ||
		TTFLoadFont(3, "sa0:data/font/pvf/cn0.pvf", NULL, 0) != SUCCESS)
		return 0;

	free_mem = (u32*) init_ttf_table((u8*) free_mem);
	set_ttf_window(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WIN_SKIP_LF);
	
	//Init Main Menu textures
	load_menu_texture(bgimg, jpg);
	load_menu_texture(cheat, png);
	load_menu_texture(leon_luna, jpg);

	load_menu_texture(circle_loading_bg, png);
	load_menu_texture(circle_loading_seek, png);
	load_menu_texture(edit_shadow, png);

	load_menu_texture(footer_ico_circle, png);
	load_menu_texture(footer_ico_cross, png);
	load_menu_texture(footer_ico_square, png);
	load_menu_texture(footer_ico_triangle, png);
	load_menu_texture(header_dot, png);
	load_menu_texture(header_line, png);

	load_menu_texture(mark_arrow, png);
	load_menu_texture(mark_line, png);
	load_menu_texture(opt_off, png);
	load_menu_texture(opt_on, png);
	load_menu_texture(scroll_bg, png);
	load_menu_texture(scroll_lock, png);
	load_menu_texture(help, png);
	load_menu_texture(buk_scr, png);
	load_menu_texture(cat_about, png);
	load_menu_texture(cat_cheats, png);
	load_menu_texture(cat_opt, png);
	load_menu_texture(cat_usb, png);
	load_menu_texture(cat_bup, png);
	load_menu_texture(cat_db, png);
	load_menu_texture(cat_hdd, png);
	load_menu_texture(cat_sav, png);
	load_menu_texture(cat_warning, png);
	load_menu_texture(column_1, png);
	load_menu_texture(column_2, png);
	load_menu_texture(column_3, png);
	load_menu_texture(column_4, png);
	load_menu_texture(column_5, png);
	load_menu_texture(column_6, png);
	load_menu_texture(column_7, png);
	load_menu_texture(jar_about, png);
	load_menu_texture(jar_about_hover, png);
	load_menu_texture(jar_bup, png);
	load_menu_texture(jar_bup_hover, png);
	load_menu_texture(jar_db, png);
	load_menu_texture(jar_db_hover, png);
	load_menu_texture(jar_trophy, png);
	load_menu_texture(jar_trophy_hover, png);
	load_menu_texture(jar_hdd, png);
	load_menu_texture(jar_hdd_hover, png);
	load_menu_texture(jar_opt, png);
	load_menu_texture(jar_opt_hover, png);
	load_menu_texture(jar_usb, png);
	load_menu_texture(jar_usb_hover, png);
	load_menu_texture(logo, png);
	load_menu_texture(logo_text, png);
	load_menu_texture(tag_lock, png);
	load_menu_texture(tag_own, png);
	load_menu_texture(tag_pce, png);
	load_menu_texture(tag_ps1, png);
	load_menu_texture(tag_vmc, png);
	load_menu_texture(tag_psp, png);
	load_menu_texture(tag_psv, png);
	load_menu_texture(tag_warning, png);
	load_menu_texture(tag_zip, png);
	load_menu_texture(tag_net, png);
	load_menu_texture(tag_apply, png);
	load_menu_texture(tag_transfer, png);

	load_menu_texture(trp_sync, png);
	load_menu_texture(trp_bronze, png);
	load_menu_texture(trp_silver, png);
	load_menu_texture(trp_gold, png);
	load_menu_texture(trp_platinum, png);

	menu_textures[icon_png_file_index].texture = NULL;

	u32 tBytes = free_mem - texture_mem;
	LOG("LoadTextures_Menu() :: Allocated %db (%.02fkb, %.02fmb) for textures", tBytes, tBytes / (float)1024, tBytes / (float)(1024 * 1024));
	return 1;
}

static int LoadSounds(void* data)
{
	uint8_t* play_audio = data;
	xmp_context xmp = xmp_create_context();

	// Decode a mp3 file to play
	if (xmp_load_module_from_memory(xmp, (void*) &_binary_data_haiku_s3m_start, (int) &_binary_data_haiku_s3m_size) < 0)
	{
		LOG("[ERROR] Failed to decode audio file");
		return -1;
	}

	xmp_set_player(xmp, XMP_PLAYER_VOLUME, 100);
	xmp_set_player(xmp, XMP_PLAYER_INTERP, XMP_INTERP_SPLINE);
	xmp_start_player(xmp, SAMPLING_FREQ, 0);

	// Calculate the sample count and allocate a buffer for the sample data accordingly
	size_t sampleCount = AUDIO_SAMPLES * 2;
	int16_t *pSampleData = (int16_t *)malloc(sampleCount * sizeof(uint16_t));

	sceAudioOutSetVolume(audio, SCE_AUDIO_VOLUME_FLAG_L_CH |SCE_AUDIO_VOLUME_FLAG_R_CH, (int[]){SCE_AUDIO_VOLUME_0DB,SCE_AUDIO_VOLUME_0DB});

	// Play the song in a loop
	while (!close_app)
	{
		if (*play_audio == 0)
		{
			usleep(0x1000);
			continue;
		}

		// Decode the audio into pSampleData
		xmp_play_buffer(xmp, pSampleData, sampleCount * sizeof(uint16_t), 0);

		/* Output audio */
		if (sceAudioOutOutput(audio, pSampleData) < 0)
		{
			LOG("Failed to output audio");
			return -1;
		}
	}

	free(pSampleData);
	xmp_end_player(xmp);
	xmp_release_module(xmp);
	xmp_free_context(xmp);

	return 0;
}

void update_usb_path(char* path)
{
	sprintf(path, USB_PATH, USER_STORAGE_DEV);

	if (dir_exists(path) == SUCCESS)
		return;

	path[0] = 0;
}

void update_hdd_path(char* path)
{
	strcpy(path, USER_PATH_HDD);
}

void update_trophy_path(char* path)
{
	sprintf(path, TROPHY_PATH_HDD "db/trophy_local.db", apollo_config.user_id);
}

void update_db_path(char* path)
{
	strcpy(path, apollo_config.save_db);
}

void update_vmc_path(char* path)
{
	if (file_exists(path) == SUCCESS)
		return;

	path[0] = 0;
}

static void registerSpecialChars(void)
{
	// Register save tags
	RegisterSpecialCharacter(CHAR_TAG_PS1, 2, 1.5, &menu_textures[tag_ps1_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_VMC, 2, 1.0, &menu_textures[tag_vmc_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PSP, 2, 1.5, &menu_textures[tag_psp_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PSV, 2, 1.5, &menu_textures[tag_psv_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PCE, 2, 1.5, &menu_textures[tag_pce_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_LOCKED, 0, 1.3, &menu_textures[tag_lock_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_OWNER, 0, 1.3, &menu_textures[tag_own_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_WARNING, 0, 1.3, &menu_textures[tag_warning_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_APPLY, 2, 1.0, &menu_textures[tag_apply_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_ZIP, 0, 1.0, &menu_textures[tag_zip_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_NET, 0, 1.0, &menu_textures[tag_net_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_TRANSFER, 0, 1.0, &menu_textures[tag_transfer_png_index]);

	// Register button icons
	RegisterSpecialCharacter(vitaPadGetConf()->crossButtonOK ? CHAR_BTN_X : CHAR_BTN_O, 0, 1.2, &menu_textures[footer_ico_cross_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_S, 0, 1.2, &menu_textures[footer_ico_square_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_T, 0, 1.2, &menu_textures[footer_ico_triangle_png_index]);
	RegisterSpecialCharacter(vitaPadGetConf()->crossButtonOK ? CHAR_BTN_O : CHAR_BTN_X, 0, 1.2, &menu_textures[footer_ico_circle_png_index]);

	// Register trophy icons
	RegisterSpecialCharacter(CHAR_TRP_BRONZE, 2, 0.9f, &menu_textures[trp_bronze_png_index]);
	RegisterSpecialCharacter(CHAR_TRP_SILVER, 2, 0.9f, &menu_textures[trp_silver_png_index]);
	RegisterSpecialCharacter(CHAR_TRP_GOLD, 2, 0.9f, &menu_textures[trp_gold_png_index]);
	RegisterSpecialCharacter(CHAR_TRP_PLATINUM, 0, 0.9f, &menu_textures[trp_platinum_png_index]);
	RegisterSpecialCharacter(CHAR_TRP_SYNC, 0, 1.2f, &menu_textures[trp_sync_png_index]);
}

static void terminate(void)
{
	LOG("Exiting...");

	sceAudioOutReleasePort(audio);
	sceKernelExitProcess(0);
}

static int initInternal(void)
{
	// load common modules
	int ret = sceSysmoduleLoadModule(SCE_SYSMODULE_SQLITE);
	if (ret != SUCCESS) {
		LOG("load module failed: SQLITE (0x%08x)\n", ret);
		return 0;
	}

	ret = sceSysmoduleLoadModule(SCE_SYSMODULE_NOTIFICATION_UTIL);
	if (ret != SUCCESS) {
		LOG("load module failed: NOTIFICATION (0x%08x)\n", ret);
		return 0;
	}

	ret = sceSysmoduleLoadModule(SCE_SYSMODULE_APPUTIL);
	if (ret != SUCCESS) {
		LOG("load module failed: APPUTIL (0x%08x)\n", ret);
		return 0;
	}

	SceAppUtilInitParam initParam;
	SceAppUtilBootParam bootParam;

	memset(&initParam, 0, sizeof(SceAppUtilInitParam));
	memset(&bootParam, 0, sizeof(SceAppUtilBootParam));

	/* Initialize the application utility library */
	ret = sceAppUtilInit(&initParam, &bootParam);
	if (ret == SUCCESS) {
		sceAppUtilSystemParamGetString(SCE_SYSTEM_PARAM_ID_USERNAME, user_id_str, SCE_SYSTEM_PARAM_USERNAME_MAXSIZE);
	}

	return 1;
}

static int initialize_vitashell_modules(void)
{
	SceUID kern_modid, user_modid;
	char module_path[60] = {0};
	int search_unk[2];
	int res = 0;

	// Load kernel module
	if (_vshKernelSearchModuleByName("VitaShellKernel2", search_unk) < 0)
	{
		snprintf(module_path, sizeof(module_path), "ux0:VitaShell/module/kernel.skprx");
		if (file_exists(module_path) != SUCCESS)
		{
			snprintf(module_path, sizeof(module_path), APOLLO_APP_PATH "sce_module/kernel.skprx");
			if (file_exists(module_path) != SUCCESS)
			{
				LOG("Kernel module not found!");
				return 0;
			}
		}

		kern_modid = taiLoadKernelModule(module_path, 0, NULL);
		if (kern_modid >= 0)
		{
			res = taiStartKernelModule(kern_modid, 0, NULL, 0, NULL, NULL);
			if (res < 0)
				taiStopUnloadKernelModule(kern_modid, 0, NULL, 0, NULL, NULL);
		}

		if (kern_modid < 0 || res < 0)
		{
			LOG("Kernel module load error: %x\nPlease reboot.", kern_modid);
			return 0;
		}
	}

	// Load user module
	snprintf(module_path, sizeof(module_path), "ux0:VitaShell/module/user.suprx");
	if (file_exists(module_path) != SUCCESS)
	{
		snprintf(module_path, sizeof(module_path), APOLLO_APP_PATH "sce_module/user.suprx");
		if (file_exists(module_path) != SUCCESS)
		{
			LOG("User module not found!");
			return 0;
		}
	}

	user_modid = sceKernelLoadStartModule(module_path, 0, NULL, 0, NULL, NULL);
	if (user_modid < 0)
	{
		LOG("User module load %s error: %x\nPlease reboot.", module_path, user_modid);
		return 0;
	}

	// Allow writing to ux0:app/NP0APOLLO
	sceAppMgrUmount("app0:");
	sceAppMgrUmount("savedata0:");

    return 1;
}


/*
	Program start
*/
s32 main(s32 argc, const char* argv[])
{
#ifdef APOLLO_ENABLE_LOGGING
	// Frame tracking info for debugging
	uint32_t lastFrameTicks  = 0;
	uint32_t startFrameTicks = 0;
	uint32_t deltaFrameTicks = 0;

	dbglogger_init();
#endif

	// Initialize SDL functions
	LOG("Initializing SDL");
	if (SDL_Init(SDL_INIT_VIDEO) != SUCCESS)
	{
		LOG("Failed to initialize SDL: %s", SDL_GetError());
		return (-1);
	}

	initInternal();
	http_init();
	initPad();

	// Open a handle to audio output device
	audio = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, AUDIO_SAMPLES, SAMPLING_FREQ, SCE_AUDIO_OUT_MODE_STEREO);
	if (audio <= 0)
	{
		LOG("[ERROR] Failed to open audio on main port");
		return audio;
	}

	// Create a window context
	LOG( "Creating a window");
	window = SDL_CreateWindow("main", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	if (!window) {
		LOG("SDL_CreateWindow: %s", SDL_GetError());
		return (-1);
	}

	// Create a renderer (OpenGL ES2)
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
	if (!renderer) {
		LOG("SDL_CreateRenderer: %s", SDL_GetError());
		return (-1);
	}

	// Initialize jailbreak
	if (!initialize_vitashell_modules())
		notification("Error loading VitaShell modules!");

	mkdirs(APOLLO_DATA_PATH);
	mkdirs(APOLLO_LOCAL_CACHE);
	
	// Load texture
	if (!LoadTextures_Menu())
	{
		notification("Failed to load menu textures!");
		return (-1);
	}

	// Load application settings
	load_app_settings(&apollo_config);

	// Unpack application data on first run
	if (file_exists(APOLLO_LOCAL_CACHE "appdata.zip") == SUCCESS)
	{
//		clean_directory(APOLLO_DATA_PATH);
		if (extract_zip(APOLLO_LOCAL_CACHE "appdata.zip", APOLLO_DATA_PATH))
			show_message("Successfully installed local application data");

		unlink_secure(APOLLO_LOCAL_CACHE "appdata.zip");
	}

	// dedicated to Leon ~ in loving memory (2009 - 2022)
	// menu_textures[buk_scr_png_index] = menu_textures[leon_jpg_index];
	// Splash screen logo (fade-in)
	drawSplashLogo(1);
 
	// Setup font
	SetExtraSpace(-10);
	SetCurrentFont(font_adonais_regular);

	registerSpecialChars();
	initMenuOptions();

	// Splash screen logo (fade-out)
	drawSplashLogo(-1);
	SDL_DestroyTexture(menu_textures[buk_scr_png_index].texture);
	
	//Set options
	update_callback(!apollo_config.update);

	// Start BGM audio thread
	SDL_CreateThread(&LoadSounds, "audio_thread", &apollo_config.music);
	Draw_MainMenu_Ani();

	while (!close_app)
	{
#ifdef APOLLO_ENABLE_LOGGING
        startFrameTicks = SDL_GetTicks();
        deltaFrameTicks = startFrameTicks - lastFrameTicks;
        lastFrameTicks  = startFrameTicks;
#endif
		// Clear the canvas
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
		SDL_RenderClear(renderer);

		vitaPadUpdate();
		drawScene();

		//Draw help
		if (menu_pad_help[menu_id])
		{
			u8 alpha = 0xFF;
			if (vitaPadGetConf()->idle > 0x100)
			{
				int dec = (vitaPadGetConf()->idle - 0x100) * 4;
				alpha = (dec > alpha) ? 0 : (alpha - dec);
			}
			
			SetFontSize(APP_FONT_SIZE_DESCRIPTION);
			SetCurrentFont(font_adonais_regular);
			SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
			SetFontColor(APP_FONT_COLOR | alpha, 0);
			DrawString(0, SCREEN_HEIGHT - 44, (char *)menu_pad_help[menu_id]);
			SetFontAlign(FONT_ALIGN_LEFT);
		}

#ifdef APOLLO_ENABLE_LOGGING
		// Calculate FPS and ms/frame
		SetFontColor(APP_FONT_COLOR | 0xFF, 0);
		DrawFormatString(50, 500, "FPS: %d", (1000 / deltaFrameTicks));
#endif
		// Propagate the updated window to the screen
		SDL_RenderPresent(renderer);
	}

	drawEndLogo();

	// Cleanup resources
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	// Stop all SDL sub-systems
	SDL_Quit();
	http_end();
	vitaPadFinish();
	terminate();

	return 0;
}
