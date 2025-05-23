#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define STBI_ASSERT(x)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "types.h"
#include "libfont.h"
#include "menu.h"

#include <dbglogger.h>
#define LOG dbglogger_log

#define JAR_COLUMNS 7


void LoadRawTexture(int idx, void* data, int width, int height)
{
	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(data, width, height, 32, 4 * width, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
	menu_textures[idx].texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);
}

int LoadMenuTexture(const char* path, int idx)
{
	int d;
	void *buffer;

	if (path)
		buffer = stbi_load(path, &menu_textures[idx].width, &menu_textures[idx].height, &d, STBI_rgb_alpha);
	else
		buffer = stbi_load_from_memory(menu_textures[idx].buffer, menu_textures[idx].size, &menu_textures[idx].width, &menu_textures[idx].height, &d, STBI_rgb_alpha);

	if (!buffer)
	{
		LOG("Error Loading texture (%s)!", path);
		return 0;
	}

	LoadRawTexture(idx, buffer, menu_textures[idx].width, menu_textures[idx].height);
	stbi_image_free(buffer);

	return 1;
}

// draw one background color in virtual 2D coordinates
void DrawBackground2D(u32 rgba)
{
	SDL_Rect rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};

	SDL_SetRenderDrawColor(renderer, RGBA_R(rgba), RGBA_G(rgba), RGBA_B(rgba), RGBA_A(rgba));
	SDL_RenderFillRect(renderer, &rect);
}

static void _drawListBackground(int off, int icon)
{
	switch (icon)
	{
		case cat_db_png_index:
		case cat_usb_png_index:
		case cat_hdd_png_index:
		case cat_opt_png_index:
		case cat_bup_png_index:
		case cat_warning_png_index:
			DrawTexture(&menu_textures[help_png_index], help_png_x, help_png_y, 0, help_png_w, help_png_h, 0xFFFFFF00 | 0xFF);
			break;

		case cat_sav_png_index:
			DrawTexture(&menu_textures[help_png_index], help_png_x, help_png_y, 0, help_png_w, help_png_h, 0xFFFFFF00 | 0xFF);

			if (menu_textures[icon_png_file_index].texture)
			{
				int off = (menu_textures[icon_png_file_index].width > 128) ? 20 : 0;
				DrawTexture(&menu_textures[help_png_index], SCREEN_WIDTH - 204 - off, help_png_y + 4, 0, menu_textures[icon_png_file_index].width + 8, menu_textures[icon_png_file_index].height + 8, 0xFFFFFF00 | 0xFF);
				DrawTexture(&menu_textures[icon_png_file_index], SCREEN_WIDTH - 200 - off, help_png_y + 8, 0, menu_textures[icon_png_file_index].width, menu_textures[icon_png_file_index].height, 0xFFFFFF00 | 0xFF);
			}
			break;

		case cat_cheats_png_index:
			DrawTexture(&menu_textures[help_png_index], off + MENU_ICON_OFF, help_png_y, 0, (SCREEN_WIDTH - 75) - off - MENU_ICON_OFF, help_png_h, 0xFFFFFF00 | 0xFF);
			break;

		case cat_about_png_index:
			break;

		default:
			break;
	}
}

void DrawHeader_Ani(int icon, const char * hdrTitle, const char * headerSubTitle, u32 rgba, u32 bgrgba, int ani, int div)
{
	u8 icon_a = (u8)(((ani * 2) > 0xFF) ? 0xFF : (ani * 2));
	char headerTitle[44];
	snprintf(headerTitle, sizeof(headerTitle), "%.40s%s", hdrTitle, (strlen(hdrTitle) > 40 ? "..." : ""));

	//------------ Backgrounds
	
	//Background
	DrawBackgroundTexture(0, (u8)bgrgba);

	_drawListBackground(0, icon);
	//------------- Menu Bar
/*
	int cnt, cntMax = ((ani * div) > (SCREEN_WIDTH - 75)) ? (SCREEN_WIDTH - 75) : (ani * div);
	for (cnt = MENU_ICON_OFF; cnt < cntMax; cnt++)
		DrawTexture(&menu_textures[header_line_png_index], cnt, 40, 0, menu_textures[header_line_png_index].width, menu_textures[header_line_png_index].height / 2, 0xffffffff);

	DrawTexture(&menu_textures[header_dot_png_index], cnt - 4, 40, 0, menu_textures[header_dot_png_index].width / 2, menu_textures[header_dot_png_index].height / 2, 0xffffff00 | icon_a);
*/

	//header mini icon
	DrawTextureCenteredX(&menu_textures[icon], MENU_ICON_OFF - 20, 32, 0, 48, 48, 0xffffff00 | icon_a);

	//header title string
	SetFontColor(rgba | icon_a, 0);
	SetFontSize(APP_FONT_SIZE_TITLE);
	DrawString(MENU_ICON_OFF + 40, 31, headerTitle);

	//header sub title string
	if (headerSubTitle)
	{
		int width = (SCREEN_WIDTH - 75) - (MENU_ICON_OFF + MENU_TITLE_OFF + WidthFromStr(headerTitle)) - 30;
		SetFontSize(APP_FONT_SIZE_SUBTITLE);
		char * tName = strdup(headerSubTitle);
		while (tName[0] && WidthFromStr(tName) > width)
		{
			tName[strlen(tName) - 1] = 0;
		}
		SetFontAlign(FONT_ALIGN_RIGHT);
		DrawString(SCREEN_WIDTH - 75, 35, tName);
		free(tName);
		SetFontAlign(FONT_ALIGN_LEFT);
	}
}

void DrawHeader(int icon, int xOff, const char * hdrTitle, const char * headerSubTitle, u32 rgba, u32 bgrgba, int mode)
{
	char headerTitle[44];
	snprintf(headerTitle, sizeof(headerTitle), "%.40s%s", hdrTitle, (strlen(hdrTitle) > 40 ? "..." : ""));

	//Background
	DrawBackgroundTexture(xOff, (u8)bgrgba);

	_drawListBackground(xOff, icon);
	//------------ Menu Bar
/*
	int cnt = 0;
	for (cnt = xOff + MENU_ICON_OFF; cnt < (SCREEN_WIDTH - 75); cnt++)
		DrawTexture(&menu_textures[header_line_png_index], cnt, 55, 0, menu_textures[header_line_png_index].width, menu_textures[header_line_png_index].height / 2, 0xffffffff);

	DrawTexture(&menu_textures[header_dot_png_index], cnt - 4, 55, 0, menu_textures[header_dot_png_index].width / 2, menu_textures[header_dot_png_index].height / 2, 0xffffffff);
*/

	//header mini icon
	//header title string
	SetFontColor(rgba, 0);
	if (mode)
	{
		DrawTextureCenteredX(&menu_textures[icon], xOff + MENU_ICON_OFF - 12, 40, 0, 32, 32, 0xffffffff);
		SetFontSize(APP_FONT_SIZE_SUBTITLE);
		DrawString(xOff + MENU_ICON_OFF + 40, 35, headerTitle);
	}
	else
	{
		DrawTextureCenteredX(&menu_textures[icon], xOff + MENU_ICON_OFF - 20, 32, 0, 48, 48, 0xffffffff);
		SetFontSize(APP_FONT_SIZE_TITLE);
		DrawString(xOff + MENU_ICON_OFF + 40, 31, headerTitle);
	}

	//header sub title string
	if (headerSubTitle)
	{
		int width = (SCREEN_WIDTH - 75) - (MENU_ICON_OFF + MENU_TITLE_OFF + WidthFromStr(headerTitle)) - 30;
		SetFontSize(APP_FONT_SIZE_SUBTITLE);
		char * tName = strdup(headerSubTitle);
		while (tName[0] && WidthFromStr(tName) > width)
		{
			tName[strlen(tName) - 1] = 0;
		}
		SetFontAlign(FONT_ALIGN_RIGHT);
		DrawString(SCREEN_WIDTH - 75, 35, tName);
		free(tName);
		SetFontAlign(FONT_ALIGN_LEFT);
	}
}

void DrawBackgroundTexture(int x, u8 alpha)
{
	DrawTexture(&menu_textures[bgimg_jpg_index], x, 0, 0, SCREEN_WIDTH - x, SCREEN_HEIGHT, 0xFFFFFF00 | alpha);
}

void DrawTexture(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba)
{
	SDL_Rect dest = {
		.x = x,
		.y = y,
		.w = w,
		.h = h,
	};

	SDL_SetTextureAlphaMod(tex->texture, RGBA_A(rgba));
	SDL_RenderCopy(renderer, tex->texture, NULL, &dest);
}

void DrawTextureCentered(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba)
{
	x -= w / 2;
	y -= h / 2;

	DrawTexture(tex, x, y, z, w, h, rgba);
}

void DrawTextureCenteredX(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba)
{
	x -= w / 2;

	DrawTexture(tex, x, y, z, w, h, rgba);
}

void DrawTextureCenteredY(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba)
{
	y -= h / 2;

	DrawTexture(tex, x, y, z, w, h, rgba);
}

void DrawTextureRotated(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba, float angle)
{
	SDL_Rect dest = {
		.x = x - (w / 2),
		.y = y - (h / 2),
		.w = w,
		.h = h,
	};

	SDL_SetTextureAlphaMod(tex->texture, RGBA_A(rgba));
	SDL_RenderCopyEx(renderer, tex->texture, NULL, &dest, angle, NULL, SDL_FLIP_NONE);
}

static int please_wait;

static int loading_screen_thread(void* user_data)
{
	float angle = 0;

	while (please_wait == 1)
	{
		angle += 4.0f;

		SDL_RenderClear(renderer);
		DrawBackground2D(0xFFFFFFFF);

		//Background
		DrawBackgroundTexture(0, 0xFF);

		//Loading animation
		DrawTextureCentered(&menu_textures[logo_png_index], SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 0, 112, 110, 0xFFFFFFFF);
		DrawTextureCentered(&menu_textures[circle_loading_bg_png_index], SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 0, 128, 128, 0xFFFFFFFF);
		DrawTextureRotated(&menu_textures[circle_loading_seek_png_index], SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 0, 128, 128, 0xFFFFFFFF, angle);

		DrawStringMono(0, 336, (char*) user_data);		

		SDL_RenderPresent(renderer);
	}

	please_wait = -1;
	return (0);
}

int init_loading_screen(const char* message)
{
	SDL_Thread* tid;
	please_wait = 1;

	SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
	SetFontSize(APP_FONT_SIZE_JARS);
	SetFontColor(APP_FONT_MENU_COLOR | 0xFF, 0);

	tid = SDL_CreateThread(&loading_screen_thread, "please_wait", (void*) message);
	SDL_DetachThread(tid);

	return (tid != NULL);
}

void stop_loading_screen(void)
{
    if (please_wait != 1)
        return;

    please_wait = 0;

    while (please_wait != -1)
        usleep(1000);
}

static void drawJar(uint8_t idx, int pos_x, int pos_y, const char* text, uint8_t alpha)
{
	uint8_t active = (menu_sel + jar_trophy_png_index == idx);
	DrawTexture(&menu_textures[idx], pos_x, pos_y, 0, menu_textures[idx].width * SCREEN_W_ADJ, menu_textures[idx].height * SCREEN_H_ADJ, 0xffffff00 | alpha);

	//Selected
	if (active)
		DrawTexture(&menu_textures[idx + JAR_COLUMNS], pos_x, pos_y, 0, menu_textures[idx + JAR_COLUMNS].width * SCREEN_W_ADJ, menu_textures[idx + JAR_COLUMNS].height * SCREEN_H_ADJ, 0xffffff00 | alpha);

	SetFontColor(APP_FONT_MENU_COLOR | (alpha == 0xFF ? (active ? 0xFF : 0x20) : alpha), 0);
	DrawStringMono(pos_x + (menu_textures[idx].width * SCREEN_W_ADJ / 2), pos_y - 30, text);
}

static void _drawColumn(uint8_t idx, int pos_x, int pos_y, uint8_t alpha)
{
	DrawTexture(&menu_textures[idx], pos_x, pos_y, 0, menu_textures[idx].width * SCREEN_W_ADJ, menu_textures[idx].height * SCREEN_H_ADJ, 0xffffff00 | alpha);
}

static void drawColumns(uint8_t alpha)
{
//	DrawTexture(&menu_textures[bg_water_png_index], bg_water_png_x, bg_water_png_y, 0, bg_water_png_w + (apollo_config.marginH * 2), bg_water_png_h, 0xffffff00 | 0xFF);

	//Columns
	_drawColumn(column_1_png_index, column_1_png_x, column_1_png_y, alpha);
	_drawColumn(column_2_png_index, column_2_png_x, column_2_png_y, alpha);
	_drawColumn(column_3_png_index, column_3_png_x, column_3_png_y, alpha);
	_drawColumn(column_4_png_index, column_4_png_x, column_4_png_y, alpha);
	_drawColumn(column_5_png_index, column_5_png_x, column_5_png_y, alpha);
	_drawColumn(column_6_png_index, column_6_png_x, column_6_png_y, alpha);
	_drawColumn(column_7_png_index, column_7_png_x, column_7_png_y, alpha);
}

static void drawJars(uint8_t alpha)
{
	SetFontAlign(FONT_ALIGN_CENTER);
	SetFontSize(APP_FONT_SIZE_JARS);
	SetCurrentFont(font_adonais_regular);

	//Trophies
	drawJar(jar_trophy_png_index, jar_empty_png_x, jar_empty_png_y, (alpha == 0xFF ? "Trophies" : ""), alpha);

	//USB save
	drawJar(jar_usb_png_index, jar_usb_png_x, jar_usb_png_y, (alpha == 0xFF ? "Ext Saves" : ""), alpha);
	
	//HDD save
	drawJar(jar_hdd_png_index, jar_hdd_png_x, jar_hdd_png_y, (alpha == 0xFF ? "Int Saves" : ""), alpha);

	//Online cheats
	drawJar(jar_db_png_index, jar_db_png_x, jar_db_png_y, (alpha == 0xFF ? (apollo_config.online_opt ? "FTP Server" : "Online DB") : ""), alpha);
	
	//User Backup
	drawJar(jar_bup_png_index, jar_bup_png_x, jar_bup_png_y, (alpha == 0xFF ? "Tools" : ""), alpha);

	//Options
	drawJar(jar_opt_png_index, jar_opt_png_x, jar_opt_png_y, (alpha == 0xFF ? "Settings" : ""), alpha);
	
	//About
	drawJar(jar_about_png_index, jar_about_png_x, jar_about_png_y, (alpha == 0xFF ? "About" : ""), alpha);

	SetFontAlign(FONT_ALIGN_LEFT);
}

void drawSplashLogo(int mode)
{
	int ani, max;

	if (mode > 0)
	{
		ani = 0;
		max = MENU_ANI_MAX;
	}
	else
	{
		ani = MENU_ANI_MAX;
		max = 0;
	}

	for (; ani != max; ani += mode)
	{
		// clear the current display buffer
		SDL_RenderClear(renderer);
		DrawBackground2D(0x000000FF);
		
		//------------ Backgrounds
		int logo_a_t = ((ani < 0x20) ? 0 : ((ani - 0x20)*3));
		if (logo_a_t > 0xFF)
			logo_a_t = 0xFF;
		u8 logo_a = (u8)logo_a_t;

		SDL_SetTextureAlphaMod(menu_textures[buk_scr_png_index].texture, logo_a);

		//App description
		DrawTextureCentered(&menu_textures[buk_scr_png_index], SCREEN_WIDTH/2, SCREEN_HEIGHT /2, 0, menu_textures[buk_scr_png_index].width * 3/4, menu_textures[buk_scr_png_index].height * 3/4, 0xFFFFFF00 | logo_a);

		//flush and flip
		SDL_RenderPresent(renderer);
	}
}

void drawEndLogo(void)
{
	SDL_Rect rect = {
		.x = 0,
		.w = SCREEN_WIDTH,
	};

	for (rect.h = 0; rect.h <= SCREEN_HEIGHT/2; rect.h += 5)
	{
		// clear the current display buffer
		SDL_RenderClear(renderer);
		DrawBackground2D(0xFFFFFFFF);

		//App description
		DrawTextureCentered(&menu_textures[logo_png_index], SCREEN_WIDTH/2, SCREEN_HEIGHT /2, 0, menu_textures[logo_png_index].width *3/4, menu_textures[logo_png_index].height *3/4, 0xFFFFFF00 | 0xFF);

		rect.y = 0;
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
		SDL_RenderFillRect(renderer, &rect);

		rect.y = SCREEN_HEIGHT - rect.h;
		SDL_RenderFillRect(renderer, &rect);

		//flush and flip
		SDL_RenderPresent(renderer);
	}
}

static void _draw_MainMenu(uint8_t alpha)
{
	//------------ Backgrounds

	//Background
	DrawBackgroundTexture(0, 0xFF);
	
	//App logo
	DrawTexture(&menu_textures[logo_png_index], logo_png_x, logo_png_y, 0, logo_png_w, logo_png_h, 0xFFFFFFFF);
	
	//App description
	DrawTextureCenteredX(&menu_textures[logo_text_png_index], SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 40, 0, menu_textures[logo_text_png_index].width, menu_textures[logo_text_png_index].height, 0xFFFFFF00 | 0xFF);

	drawColumns(alpha);

	//------------ Icons
	drawJars(alpha);
}

void Draw_MainMenu_Ani(void)
{
	int max = MENU_ANI_MAX, ani = 0;
	for (ani = 0; ani < max; ani++)
	{
		SDL_RenderClear(renderer);
		DrawBackground2D(0xFFFFFFFF);
		
		//------------ Backgrounds
		u8 bg_a = (u8)(ani * 2);
		if (bg_a < 0x20)
			bg_a = 0x20;
		int logo_a_t = ((ani < 0x30) ? 0 : ((ani - 0x20)*3));
		if (logo_a_t > 0xFF)
			logo_a_t = 0xFF;
		u8 logo_a = (u8)logo_a_t;
		
		//Background
		DrawBackgroundTexture(0, bg_a);
		
		//App logo
		DrawTexture(&menu_textures[logo_png_index], logo_png_x, logo_png_y, 0, logo_png_w, logo_png_h, 0xFFFFFF00 | logo_a);
		
		//App description
		DrawTextureCenteredX(&menu_textures[logo_text_png_index], SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 40, 0, menu_textures[logo_text_png_index].width, menu_textures[logo_text_png_index].height, 0xFFFFFF00 | logo_a);

		SDL_RenderPresent(renderer);
	}
	
	max = MENU_ANI_MAX / 2;
	int rate = (0x100 / max);
	for (ani = 0; ani < max; ani++)
	{
		SDL_RenderClear(renderer);
		DrawBackground2D(0xFFFFFFFF);
		
		u8 icon_a = (u8)(((ani * rate) > 0xFF) ? 0xFF : (ani * rate));
		
		_draw_MainMenu(icon_a);
		
		SDL_RenderPresent(renderer);

		if (icon_a == 32)
			break;
	}
}

void Draw_MainMenu(void)
{
	_draw_MainMenu(0xFF);
}

void drawDialogBackground(void)
{
	SDL_RenderClear(renderer);
	DrawBackground2D(0xFFFFFFFF);

	//Background
	DrawBackgroundTexture(0, 0xFF);

	SDL_RenderPresent(renderer);
}
