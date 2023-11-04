#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <psp2/ime_dialog.h>
#include <psp2/message_dialog.h>
#include <psp2/notificationutil.h>
#include <psp2/kernel/processmgr.h>
#include <SDL2/SDL.h>

#include "menu.h"

#define SCE_NOTIFICATION_UTIL_TEXT_MAX                     (0x3F)

typedef struct SceNotificationUtilSendParam {
    SceWChar16 text[SCE_NOTIFICATION_UTIL_TEXT_MAX];         // must be null-terminated
    SceInt16 separator;                                      // must be 0
    SceChar8 unknown[0x3F0];
} SceNotificationUtilSendParam;

static int g_ime_active;

static uint16_t g_ime_title[SCE_IME_DIALOG_MAX_TITLE_LENGTH];
static uint16_t g_ime_text[SCE_IME_DIALOG_MAX_TEXT_LENGTH];
static uint16_t g_ime_input[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];

int show_dialog(int tdialog, const char * format, ...)
{
    SceMsgDialogParam param;
    SceMsgDialogUserMessageParam user_message_param;
    SceMsgDialogResult result;
    char str[SCE_MSG_DIALOG_USER_MSG_SIZE];
    va_list	opt;

    memset(str, 0, sizeof(str));
    va_start(opt, format);
    vsnprintf(str, sizeof(str), format, opt);
    va_end(opt);

    memset(&user_message_param, 0, sizeof(SceMsgDialogUserMessageParam));
    user_message_param.msg = (SceChar8 *) str;
    user_message_param.buttonType = (tdialog ? SCE_MSG_DIALOG_BUTTON_TYPE_YESNO : SCE_MSG_DIALOG_BUTTON_TYPE_OK);

    sceMsgDialogParamInit(&param);
    param.userMsgParam = &user_message_param;
    param.mode = SCE_MSG_DIALOG_MODE_USER_MSG;

    if (sceMsgDialogInit(&param) < 0)
        return 0;

    do {
        if (tdialog == DIALOG_TYPE_OK)
            sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);

        drawDialogBackground();
    } while (sceMsgDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED);
    sceMsgDialogClose();

    memset(&result, 0, sizeof(SceMsgDialogResult));
    sceMsgDialogGetResult(&result);
    sceMsgDialogTerm();

    return (result.buttonId == SCE_MSG_DIALOG_BUTTON_ID_YES);
}

static int running = 0;

static int update_screen_thread(void* user_data)
{
    while (running == 1)
        drawDialogBackground();

    running = -1;
    return (0);
}

void init_progress_bar(const char* msg)
{
    SceMsgDialogParam param;
    SceMsgDialogProgressBarParam progress_bar_param;

    memset(&progress_bar_param, 0, sizeof(SceMsgDialogProgressBarParam));
    progress_bar_param.barType = SCE_MSG_DIALOG_PROGRESSBAR_TYPE_PERCENTAGE;
    progress_bar_param.msg = (SceChar8 *) msg;

    sceMsgDialogParamInit(&param);
    param.progBarParam = &progress_bar_param;
    param.mode = SCE_MSG_DIALOG_MODE_PROGRESS_BAR;

    if (sceMsgDialogInit(&param) < 0)
        return;

    running = 1;
    SDL_Thread* tid = SDL_CreateThread(&update_screen_thread, "progress_bar", NULL);
    SDL_DetachThread(tid);
}

void end_progress_bar(void)
{
    SceCommonDialogStatus stat;

    do
    {
        sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, 100);
        stat = sceMsgDialogGetStatus();

        if(stat == SCE_COMMON_DIALOG_STATUS_RUNNING)
            sceMsgDialogClose();

    } while (stat != SCE_COMMON_DIALOG_STATUS_FINISHED);
    
    sceMsgDialogTerm();
    running = 0;
}

void update_progress_bar(uint64_t progress, const uint64_t total_size, const char* msg)
{
    float bar_value = (100.0f * ((double) progress)) / ((double) total_size);

    if (sceMsgDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING)
    {
        sceMsgDialogProgressBarSetMsg(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, (SceChar8*) msg);
        sceMsgDialogProgressBarSetValue(SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT, (SceUInt32) bar_value);
    }
}

static void strWChar16ncpy(SceWChar16* out, const char* str2, int len)
{
    char* str1 = (char*) out;

    while (*str2 && len--)
    {
        *str1 = *str2;
        str1++;
        *str1 = '\0';
        str1++;
        str2++;
    }
}

void notification(const char *p_Format, ...)
{
    SceNotificationUtilSendParam param;
    char buf[SCE_NOTIFICATION_UTIL_TEXT_MAX];

    va_list p_Args;
    va_start(p_Args, p_Format);
    vsnprintf(buf, sizeof(buf), p_Format, p_Args);
    va_end(p_Args);

    memset(&param, 0, sizeof(SceNotificationUtilSendParam));
    strWChar16ncpy(param.text, buf, SCE_NOTIFICATION_UTIL_TEXT_MAX);

    sceNotificationUtilSendNotification((void*) &param);
}

static int convert_to_utf16(const char* utf8, uint16_t* utf16, uint32_t available)
{
    int count = 0;
    while (*utf8)
    {
        uint8_t ch = (uint8_t)*utf8++;
        uint32_t code;
        uint32_t extra;

        if (ch < 0x80)
        {
            code = ch;
            extra = 0;
        }
        else if ((ch & 0xe0) == 0xc0)
        {
            code = ch & 31;
            extra = 1;
        }
        else if ((ch & 0xf0) == 0xe0)
        {
            code = ch & 15;
            extra = 2;
        }
        else
        {
            // TODO: this assumes there won't be invalid utf8 codepoints
            code = ch & 7;
            extra = 3;
        }

        for (uint32_t i=0; i<extra; i++)
        {
            uint8_t next = (uint8_t)*utf8++;
            if (next == 0 || (next & 0xc0) != 0x80)
            {
                return count;
            }
            code = (code << 6) | (next & 0x3f);
        }

        if (code < 0xd800 || code >= 0xe000)
        {
            if (available < 1) return count;
            utf16[count++] = (uint16_t)code;
            available--;
        }
        else // surrogate pair
        {
            if (available < 2) return count;
            code -= 0x10000;
            utf16[count++] = 0xd800 | (code >> 10);
            utf16[count++] = 0xdc00 | (code & 0x3ff);
            available -= 2;
        }
    }
    utf16[count]=0;
    return count;
}

static int convert_from_utf16(const uint16_t* utf16, char* utf8, uint32_t size)
{
    int count = 0;
    while (*utf16)
    {
        uint32_t code;
        uint16_t ch = *utf16++;
        if (ch < 0xd800 || ch >= 0xe000)
        {
            code = ch;
        }
        else // surrogate pair
        {
            uint16_t ch2 = *utf16++;
            if (ch < 0xdc00 || ch > 0xe000 || ch2 < 0xd800 || ch2 > 0xdc00)
            {
                return count;
            }
            code = 0x10000 + ((ch & 0x03FF) << 10) + (ch2 & 0x03FF);
        }

        if (code < 0x80)
        {
            if (size < 1) return count;
            utf8[count++] = (char)code;
            size--;
        }
        else if (code < 0x800)
        {
            if (size < 2) return count;
            utf8[count++] = (char)(0xc0 | (code >> 6));
            utf8[count++] = (char)(0x80 | (code & 0x3f));
            size -= 2;
        }
        else if (code < 0x10000)
        {
            if (size < 3) return count;
            utf8[count++] = (char)(0xe0 | (code >> 12));
            utf8[count++] = (char)(0x80 | ((code >> 6) & 0x3f));
            utf8[count++] = (char)(0x80 | (code & 0x3f));
            size -= 3;
        }
        else
        {
            if (size < 4) return count;
            utf8[count++] = (char)(0xf0 | (code >> 18));
            utf8[count++] = (char)(0x80 | ((code >> 12) & 0x3f));
            utf8[count++] = (char)(0x80 | ((code >> 6) & 0x3f));
            utf8[count++] = (char)(0x80 | (code & 0x3f));
            size -= 4;
        }
    }
    utf8[count]=0;
    return count;
}

static int osk_dialog_input_init(int tosk, const char* title, const char* text, uint32_t maxlen)
{
    SceImeDialogParam param;
    sceImeDialogParamInit(&param);

    convert_to_utf16(title, g_ime_title, countof(g_ime_title) - 1);
    convert_to_utf16(text, g_ime_text, countof(g_ime_text) - 1);
    memset(g_ime_input, 0, sizeof(g_ime_input));
    g_ime_active = 0;

    param.supportedLanguages = 0x0001FFFF;
    param.type = (tosk ? SCE_IME_TYPE_URL : SCE_IME_TYPE_DEFAULT);
    param.option = (tosk ? SCE_IME_OPTION_NO_AUTO_CAPITALIZATION : 0);
    param.title = g_ime_title;
    param.maxTextLength = maxlen;
    param.initialText = g_ime_text;
    param.inputTextBuffer = g_ime_input;

    if (sceImeDialogInit(&param) < 0)
        return 0;

    g_ime_active = 1;
    return 1;
}

static int osk_dialog_input_update(void)
{
    if (!g_ime_active)
    {
        return 0;
    }

    SceCommonDialogStatus status = sceImeDialogGetStatus();
    if (status == SCE_COMMON_DIALOG_STATUS_FINISHED)
    {
        SceImeDialogResult result = { 0 };
        sceImeDialogGetResult(&result);

        g_ime_active = 0;
        sceImeDialogTerm();

        if (result.button == SCE_IME_DIALOG_BUTTON_ENTER)
        {
            return 1;
        }

        return (-1);
    }

    return 0;
}

int osk_dialog_get_text(const char* title, char* text, uint32_t size)
{
    size = (size > SCE_IME_DIALOG_MAX_TEXT_LENGTH) ? SCE_IME_DIALOG_MAX_TEXT_LENGTH : (size-1);

    if (!osk_dialog_input_init(1, title, text, size))
        return 0;

    while (g_ime_active)
    {
        if (osk_dialog_input_update() < 0)
            return 0;

        drawDialogBackground();
    }

    return (convert_from_utf16(g_ime_input, text, size));
}
