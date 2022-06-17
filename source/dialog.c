#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <psp2/message_dialog.h>
#include <psp2/notificationutil.h>

#define SCE_NOTIFICATION_UTIL_TEXT_MAX                     (0x3F)

typedef struct SceNotificationUtilSendParam {
    SceWChar16 text[SCE_NOTIFICATION_UTIL_TEXT_MAX];         // must be null-terminated
    SceInt16 separator;                                      // must be 0
    SceChar8 unknown[0x3F0];
} SceNotificationUtilSendParam;


void drawDialogBackground();

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
        drawDialogBackground();
    } while (sceMsgDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED);
    sceMsgDialogClose();

    memset(&result, 0, sizeof(SceMsgDialogResult));
    sceMsgDialogGetResult(&result);
    sceMsgDialogTerm();

    return (result.buttonId == SCE_MSG_DIALOG_BUTTON_ID_YES);
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

    drawDialogBackground();
}

void end_progress_bar(void)
{
    sceMsgDialogClose();
    sceMsgDialogTerm();
}

void update_progress_bar(uint64_t progress, const uint64_t total_size, const char* msg)
{
    float bar_value = (100.0f * ((double) progress)) / ((double) total_size);

    if (sceMsgDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING)
    {
        sceMsgDialogProgressBarSetMsg(0, (SceChar8*) msg);
        sceMsgDialogProgressBarSetValue(0, (SceUInt32) bar_value);
    }

    drawDialogBackground();
}

void strWChar16ncpy(SceWChar16* out, const char* str2, int len)
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
