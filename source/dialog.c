#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <psp2/message_dialog.h>


void drawDialogBackground();

int show_dialog(int tdialog, const char * format, ...)
{
    SceMsgDialogParam param;
    SceMsgDialogUserMessageParam user_message_param;
    SceMsgDialogResult result;
    char str[512];
    va_list	opt;

    memset(str, 0, sizeof(str));
    va_start(opt, format);
    vsprintf((void*) str, format, opt);
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

int init_loading_screen(const char* message)
{
    init_progress_bar(message);
    return 1;
}

void stop_loading_screen()
{
    update_progress_bar(1, 1, "");
    end_progress_bar();
}

void notifi(const char *p_Uri, const char *p_Format, ...)
{
/*
    OrbisNotificationRequest s_Request;
    memset(&s_Request, '\0', sizeof(s_Request));

    s_Request.reqId = NotificationRequest;
    s_Request.unk3 = 0;
    s_Request.useIconImageUri = 1;
    s_Request.targetId = -1;

    // Maximum size to move is destination size - 1 to allow for null terminator
    if (p_Uri != NULL && strnlen(p_Uri, sizeof(s_Request.iconUri)) + 1 > sizeof(s_Request.iconUri)) {
        strncpy(s_Request.iconUri, p_Uri, strnlen(p_Uri, sizeof(s_Request.iconUri) - 1));
    } else {
        s_Request.useIconImageUri = 0;
    }

    va_list p_Args;
    va_start(p_Args, p_Format);
    // p_Format is controlled externally, some compiler/linter options will mark this as a security issue
    vsnprintf(s_Request.message, sizeof(s_Request.message), p_Format, p_Args);
    va_end(p_Args);

    sceKernelSendNotificationRequest(NotificationRequest, &s_Request, sizeof(s_Request), 0);
*/
}
