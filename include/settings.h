#define APOLLO_VERSION          "1.1.2"     //Apollo Vita version (about menu)

#define MENU_TITLE_OFF			30			//Offset of menu title text from menu mini icon
#define MENU_ICON_OFF 			70          //X Offset to start printing menu mini icon
#define MENU_ANI_MAX 			0x80        //Max animation number
#define MENU_SPLIT_OFF			200			//Offset from left of sub/split menu to start drawing
#define OPTION_ITEM_OFF         730         //Offset from left of settings item/value

enum app_option_type
{
    APP_OPTION_NONE,
    APP_OPTION_BOOL,
    APP_OPTION_LIST,
    APP_OPTION_INC,
    APP_OPTION_CALL,
};

typedef struct
{
	char * name;
	char * * options;
	int type;
	uint8_t * value;
	void(*callback)(int);
} menu_option_t;

typedef struct __attribute__((packed))
{
    uint8_t music;
    uint8_t doSort;
    uint8_t doAni;
    uint8_t storage;
    uint8_t update;
    uint32_t user_id;
    uint64_t idps[2];
    uint64_t account_id;
} app_config_t;

extern menu_option_t menu_options[];

extern app_config_t apollo_config;

void log_callback(int sel);
void owner_callback(int sel);
void music_callback(int sel);
void sort_callback(int sel);
void ani_callback(int sel);
void update_callback(int sel);
void clearcache_callback(int sel);
void upd_appdata_callback(int sel);
void unzip_app_data(const char* zip_file);

int save_xml_owner(const char *xmlfile);
int read_xml_owner(const char *xmlfile, const char *owner);
int get_xml_owners(const char *xmlfile, int cmd, char*** nam, char*** val);
