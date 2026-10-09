#include <string.h>

#include <psp2/apputil.h>
#include <psp2/common_dialog.h>
#include <psp2/ctrl.h>
#include <psp2/ime_dialog.h>
#include <psp2/kernel/threadmgr.h>

#include "key.h"
#include "vita.h"

#define IME_MAX_LEN		64

static int dialog_ready;

// common dialogs refuse to open until apputil and the dialog config are set up
static void dialog_init(void)
{
	SceAppUtilInitParam init;
	SceAppUtilBootParam boot;
	SceCommonDialogConfigParam config;

	memset(&init, 0, sizeof(init));
	memset(&boot, 0, sizeof(boot));
	sceAppUtilInit(&init, &boot);

	sceCommonDialogConfigParamInit(&config);
	sceCommonDialogSetConfigParam(&config);

	dialog_ready = 1;
}

static void to_utf16(SceWChar16 *dst, const char *src, int max)
{
	int i;

	for (i = 0; i < max - 1 && src[i]; i++)
		dst[i] = (src[i] == '\n') ? ' ' : (unsigned char)src[i];
	dst[i] = 0;
}

// blocks until the keyboard closes, the scanout thread is the one drawing it.
// 1 means the player hit Enter and text holds the new string
int vita_ime_input(const char *title, char *text, int max_len)
{
	SceImeDialogParam param;
	SceImeDialogResult result;
	SceWChar16 title16[SCE_IME_DIALOG_MAX_TITLE_LENGTH];
	SceWChar16 initial[IME_MAX_LEN + 1];
	SceWChar16 input[IME_MAX_LEN + 1];
	int i, ok;

	if (!dialog_ready)
		dialog_init();

	if (max_len > IME_MAX_LEN)
		max_len = IME_MAX_LEN;

	to_utf16(title16, title ? title : "", SCE_IME_DIALOG_MAX_TITLE_LENGTH);
	to_utf16(initial, text, max_len + 1);
	memset(input, 0, sizeof(input));

	sceImeDialogParamInit(&param);
	param.supportedLanguages = SCE_IME_LANGUAGE_ENGLISH;
	param.languagesForced = SCE_TRUE;
	param.type = SCE_IME_TYPE_BASIC_LATIN;
	param.option = SCE_IME_OPTION_NO_AUTO_CAPITALIZATION;
	param.textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR;
	param.title = title16;
	param.maxTextLength = max_len;
	param.initialText = initial;
	param.inputTextBuffer = input;

	if (sceImeDialogInit(&param) < 0)
		return 0;

	while (sceImeDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED)
		sceKernelDelayThread(16 * 1000);

	memset(&result, 0, sizeof(result));
	sceImeDialogGetResult(&result);
	sceImeDialogTerm();

	ok = (result.button == SCE_IME_DIALOG_BUTTON_ENTER);
	if (ok) {
		for (i = 0; i < max_len && input[i]; i++)
			text[i] = (input[i] < 128) ? input[i] : '_';
		text[i] = 0;
	}

	// the button that closed the keyboard is often still down; let it go first
	// or the next menu takes it as a press
	for (i = 0; i < 100; i++) {
		SceCtrlData pad;

		memset(&pad, 0, sizeof(pad));
		sceCtrlPeekBufferPositive(0, &pad, 1);
		if (!pad.buttons)
			break;
		sceKernelDelayThread(10 * 1000);
	}
	key_flush();

	return ok;
}
