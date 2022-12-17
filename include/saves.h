#include <apollo.h>
#include <dbglogger.h>
#define LOG dbglogger_log

#define APOLLO_PATH				"ux0:/data/apollo/"
#define APOLLO_APP_PATH			"ux0:app/NP0APOLLO/"
#define APOLLO_SANDBOX_PATH		"ux0:user/00/savedata/%s/"
#define APOLLO_USER_PATH		APOLLO_PATH "%02x/"
#define APOLLO_DATA_PATH		APOLLO_APP_PATH "data/"
#define APOLLO_LOCAL_CACHE		APOLLO_APP_PATH "cache/"
#define APOLLO_UPDATE_URL		"https://api.github.com/repos/bucanero/apollo-vita/releases/latest"

#define MAX_USB_DEVICES         5
#define UX0_PATH                "ux0:data/"
#define UMA0_PATH               "uma0:data/"
#define IMC0_PATH               "imc0:data/"
#define USB_PATH                "%s:data/savegames/"
#define USER_PATH_HDD           "ur0:shell/db/app.db"

#define PS2_SAVES_PATH_USB      "PS3/EXPORT/PS2SD/"
#define PSP_SAVES_PATH_USB      "PSP/SAVEDATA/"
#define PSV_SAVES_PATH_USB      "savegames/"
#define TROPHIES_PATH_USB       "trophies/"

#define PSV_LICENSE_PATH        "ux0:license/"
#define PSV_ICONS_PATH_HDD      "ur0:appmeta/%s"
#define PS1_SAVES_PATH_HDD      APOLLO_PATH "PS1/"
#define PSP_SAVES_PATH_HDD      "ux0:pspemu/" PSP_SAVES_PATH_USB

#define PS1_IMP_PATH_USB        "PS1/SAVEDATA/"
#define PS2_IMP_PATH_USB        "PS2/SAVEDATA/"

#define SAVES_PATH_UMA0         UMA0_PATH PSV_SAVES_PATH_USB
#define SAVES_PATH_IMC0         IMC0_PATH PSV_SAVES_PATH_USB

#define TROPHY_PATH_UMA0        UMA0_PATH TROPHIES_PATH_USB
#define TROPHY_PATH_IMC0        IMC0_PATH TROPHIES_PATH_USB
#define TROPHY_PATH_HDD         "ur0:/user/%02x/trophy/"

#define EXPORT_PATH             "savegames/EXPORT/"
#define EXPORT_ZRIF_PATH        APOLLO_PATH "zrif/"

#define EXP_PSP_PATH_UMA0       UMA0_PATH PSP_SAVES_PATH_USB
#define EXP_PSP_PATH_IMC0       IMC0_PATH PSP_SAVES_PATH_USB

#define IMP_PS2VMC_PATH_USB     USB_PATH "PS2/VMC/"
#define IMPORT_RAP_PATH_USB     USB_PATH PS3_LICENSE_PATH

#define ONLINE_URL				"https://bucanero.github.io/apollo-saves/"
#define ONLINE_PATCH_URL		"https://bucanero.github.io/apollo-patches/PSV/"
#define ONLINE_CACHE_TIMEOUT    24*3600     // 1-day local cache

#define OWNER_XML_FILE          "owners.xml"

enum storage_enum
{
    STORAGE_UMA0,
    STORAGE_IMC0,
    STORAGE_UX0,
};

enum save_sort_enum
{
    SORT_DISABLED,
    SORT_BY_NAME,
    SORT_BY_TITLE_ID,
};

enum cmd_code_enum
{
    CMD_CODE_NULL,

// Trophy commands
    CMD_COPY_ALL_TROP_USB,
    CMD_EXP_TROPHY_USB,
    CMD_COPY_TROPHIES_USB,
    CMD_ZIP_TROPHY_USB,

// Save commands
    CMD_DECRYPT_FILE,
    CMD_RESIGN_SAVE,
    CMD_DOWNLOAD_USB,
    CMD_DOWNLOAD_HDD,
    CMD_COPY_SAVE_USB,
    CMD_COPY_SAVE_HDD,
    CMD_EXPORT_ZIP_USB,
    CMD_VIEW_DETAILS,
    CMD_VIEW_RAW_PATCH,
    CMD_RESIGN_VMP,
    CMD_EXP_FINGERPRINT,
    CMD_CONVERT_TO_PSV,
    CMD_COPY_PFS,
    CMD_IMPORT_DATA_FILE,

// Bulk commands
    CMD_RESIGN_SAVES,
    CMD_RESIGN_ALL_SAVES,
    CMD_COPY_SAVES_USB,
    CMD_COPY_ALL_SAVES_USB,
    CMD_COPY_SAVES_HDD,
    CMD_COPY_ALL_SAVES_HDD,
    CMD_DUMP_FINGERPRINTS,
    CMD_RUN_WEBSERVER,

// Export commands
    CMD_EXP_KEYSTONE,
    CMD_EXP_LIC_ZRIF,
    CMD_EXP_VMP2MCR,
    CMD_EXP_PSPKEY,
    CMD_DUMP_PSPKEY,

// Import commands
    CMD_IMP_KEYSTONE,
    CMD_IMP_MCR2VMP0,
    CMD_IMP_MCR2VMP1,
    CMD_EXTRACT_ARCHIVE,

// SFO patches
    SFO_UNLOCK_COPY,
    SFO_CHANGE_ACCOUNT_ID,
    SFO_REMOVE_PSID,
    SFO_CHANGE_TITLE_ID,
};

// Save flags
#define SAVE_FLAG_LOCKED        1
#define SAVE_FLAG_OWNER         2
#define SAVE_FLAG_SELECTED      4
#define SAVE_FLAG_ZIP           8
#define SAVE_FLAG_PS2           16
#define SAVE_FLAG_PSP           32
#define SAVE_FLAG_PSV           64
#define SAVE_FLAG_TROPHY        128
#define SAVE_FLAG_ONLINE        256
#define SAVE_FLAG_PS1           512
#define SAVE_FLAG_HDD           1024

enum save_type_enum
{
    FILE_TYPE_NULL,
    FILE_TYPE_PSV,
    FILE_TYPE_TRP,
    FILE_TYPE_MENU,
    FILE_TYPE_PSP,

    // PS1 File Types
    FILE_TYPE_ZIP,

    // License Files
    FILE_TYPE_RIF,
    FILE_TYPE_RAP,
    FILE_TYPE_ACT,

    // ISO Files
    FILE_TYPE_ISO,
    FILE_TYPE_BINENC,
};

enum char_flag_enum
{
    CHAR_TAG_NULL,
    CHAR_TAG_PS1,
    CHAR_TAG_PS2,
    CHAR_TAG_PS3,
    CHAR_TAG_PSP,
    CHAR_TAG_PSV,
    CHAR_TAG_APPLY,
    CHAR_TAG_OWNER,
    CHAR_TAG_LOCKED,
    CHAR_RES_TAB,
    CHAR_RES_LF,
    CHAR_TAG_TRANSFER,
    CHAR_TAG_ZIP,
    CHAR_RES_CR,
    CHAR_TAG_PCE,
    CHAR_TAG_WARNING,
    CHAR_BTN_X,
    CHAR_BTN_S,
    CHAR_BTN_T,
    CHAR_BTN_O,
    CHAR_TRP_BRONZE,
    CHAR_TRP_SILVER,
    CHAR_TRP_GOLD,
    CHAR_TRP_PLATINUM,
    CHAR_TRP_SYNC,
    CHAR_TAG_PS4,
};

enum code_type_enum
{
    PATCH_NULL,
    PATCH_GAMEGENIE = APOLLO_CODE_GAMEGENIE,
    PATCH_BSD = APOLLO_CODE_BSD,
    PATCH_COMMAND,
    PATCH_SFO,
    PATCH_TROP_UNLOCK,
    PATCH_TROP_LOCK,
};

typedef struct save_entry
{
    char * name;
	char * title_id;
	char * path;
	char * dir_name;
    uint32_t blocks;
	uint16_t flags;
    uint16_t type;
    list_t * codes;
} save_entry_t;

typedef struct
{
    list_t * list;
    char path[128];
    char* title;
    uint8_t icon_id;
    void (*UpdatePath)(char *);
    int (*ReadCodes)(save_entry_t *);
    list_t* (*ReadList)(const char*);
} save_list_t;

list_t * ReadUsbList(const char* userPath);
list_t * ReadUserList(const char* userPath);
list_t * ReadOnlineList(const char* urlPath);
list_t * ReadBackupList(const char* userPath);
list_t * ReadTrophyList(const char* userPath);
void UnloadGameList(list_t * list);
char * readTextFile(const char * path, long* size);
int sortSaveList_Compare(const void* A, const void* B);
int sortSaveList_Compare_TitleID(const void* A, const void* B);
int sortCodeList_Compare(const void* A, const void* B);
int ReadCodes(save_entry_t * save);
int ReadTrophies(save_entry_t * game);
int ReadOnlineSaves(save_entry_t * game);
int ReadBackupCodes(save_entry_t * bup);

int http_init(void);
void http_end(void);
int http_download(const char* url, const char* filename, const char* local_dst, int show_progress);

int extract_7zip(const char* zip_file, const char* dest_path);
int extract_rar(const char* rar_file, const char* dest_path);
int extract_zip(const char* zip_file, const char* dest_path);
int zip_directory(const char* basedir, const char* inputdir, const char* output_zipfile);
int zip_append_directory(const char* basedir, const char* inputdir, const char* output_filename);

int show_dialog(int dialog_type, const char * format, ...);
void init_progress_bar(const char* msg);
void update_progress_bar(uint64_t progress, const uint64_t total_size, const char* msg);
void end_progress_bar(void);
#define show_message(...)	show_dialog(0, __VA_ARGS__)

int init_loading_screen(const char* msg);
void stop_loading_screen();
void disable_unpatch();

void execCodeCommand(code_entry_t* code, const char* codecmd);

int patch_trophy_account(const char* trp_path, const char* account_id);
int apply_trophy_patch(const char* trp_path, uint32_t trophy_id, int unlock);

int make_key_zrif(const char *rif_path, const char *out_path);
int regMgr_GetUserName(int userNumber, char* outString);
int regMgr_GetAccountId(int userNumber, uint64_t* psnAccountId);
int regMgr_SetAccountId(int userNumber, uint64_t* psnAccountId);

int create_savegame_folder(const char* folder);
int get_save_details(const save_entry_t *save, char** details);
int vita_SaveUmount();
int vita_SaveMount(const save_entry_t *save);
int orbis_UpdateSaveParams(const char* mountPath, const char* title, const char* subtitle, const char* details);

int read_psp_game_key(const char* fkey, uint8_t* key);
int psp_DecryptSavedata(const char* fpath, const char* fname, uint8_t* key);
int psp_EncryptSavedata(const char* fpath, const char* fname, uint8_t* key);

int vmp_resign(const char *src_vmp);
int ps1_mcr2vmp(const char* mcrfile, const char* vmp_path);
int ps1_vmp2mcr(const char* vmpfile, const char* mcr_path);
