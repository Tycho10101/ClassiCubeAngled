#include "Core.h"
#if defined CC_BUILD_3DS
#include "Window.h"
#include "Platform.h"
#include "Input.h"
#include "Event.h"
#include "Graphics.h"
#include "String.h"
#include "Funcs.h"
#include "Bitmap.h"
#include "Errors.h"
#include "ExtMath.h"
#include "Gui.h"
#include <3ds.h>

static cc_bool launcherMode;
static Result irrst_result;
static u16 top_width, top_height;
static u16 btm_width, btm_height;

struct _DisplayData DisplayInfo;
struct _WindowData WindowInfo;
struct _WindowData Window_Alt;
cc_bool launcherTop;

// Note from https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/gfx.h
//  * Please note that the 3DS uses *portrait* screens rotated 90 degrees counterclockwise.
//  * Width/height refer to the physical dimensions of the screen; that is, the top screen
//  * is 240 pixels wide and 400 pixels tall; while the bottom screen is 240x320.
void Window_Init(void) {
	gfxInit(GSP_BGR8_OES, GSP_BGR8_OES, false);
	
	// deliberately swapped
	gfxGetFramebuffer(GFX_TOP,    GFX_LEFT, &top_height, &top_width);
	gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &btm_height, &btm_width);
	
	DisplayInfo.Width  = top_width; 
	DisplayInfo.Height = top_height;
	DisplayInfo.ScaleX = 0.5f;
	DisplayInfo.ScaleY = 0.5f;
	
	Window_Main.Width   = top_width;
	Window_Main.Height  = top_height;
	Window_Main.Focused = true;
	Window_Main.Exists  = true;

	Input_SetTouchMode(true);
	Gui_SetTouchUI(true);
	Input.Sources = INPUT_SOURCE_GAMEPAD;
	irrst_result  = irrstInit();
	
	Window_Alt.Width  = btm_width;
	Window_Alt.Height = btm_height;
}

void Window_Free(void) { irrstExit(); }

void Window_Create2D(int width, int height) {  
	DisplayInfo.Width = btm_width;
	Window_Main.Width = btm_width;
	Window_Alt.Width  = top_width;
	launcherMode      = true;  
}

void Window_Create3D(int width, int height) { 
	DisplayInfo.Width = top_width;
	Window_Main.Width = top_width;
	Window_Alt.Width  = btm_width;
	launcherMode      = false; 
}

void Window_SetTitle(const cc_string* title) { }
void Clipboard_GetText(cc_string* value) { }
void Clipboard_SetText(const cc_string* value) { }

int Window_GetWindowState(void) { return WINDOW_STATE_FULLSCREEN; }
cc_result Window_EnterFullscreen(void) { return 0; }
cc_result Window_ExitFullscreen(void)  { return 0; }
int Window_IsObscured(void)            { return 0; }

void Window_Show(void) { }
void Window_SetSize(int width, int height) { }

void Window_RequestClose(void) {
	Event_RaiseVoid(&WindowEvents.Closing);
}

/*########################################################################################################################*
*----------------------------------------------------Input processing-----------------------------------------------------*
*#########################################################################################################################*/
static void HandleButtons(u32 mods) {
	Gamepad_SetButton(CCPAD_L, mods & KEY_L);
	Gamepad_SetButton(CCPAD_R, mods & KEY_R);
	
	Gamepad_SetButton(CCPAD_A, mods & KEY_A);
	Gamepad_SetButton(CCPAD_B, mods & KEY_B);
	Gamepad_SetButton(CCPAD_X, mods & KEY_X);
	Gamepad_SetButton(CCPAD_Y, mods & KEY_Y);
	
	Gamepad_SetButton(CCPAD_START,  mods & KEY_START);
	Gamepad_SetButton(CCPAD_SELECT, mods & KEY_SELECT);
	
	Gamepad_SetButton(CCPAD_LEFT,   mods & KEY_DLEFT);
	Gamepad_SetButton(CCPAD_RIGHT,  mods & KEY_DRIGHT);
	Gamepad_SetButton(CCPAD_UP,     mods & KEY_DUP);
	Gamepad_SetButton(CCPAD_DOWN,   mods & KEY_DDOWN);
	
	Gamepad_SetButton(CCPAD_ZL, mods & KEY_ZL);
	Gamepad_SetButton(CCPAD_ZR, mods & KEY_ZR);
}

#define AXIS_SCALE 8.0f
static void ProcessCircleInput(int axis, circlePosition* pos, double delta) {
	// May not be exactly 0 on actual hardware
	if (Math_AbsI(pos->dx) <= 16) pos->dx = 0;
	if (Math_AbsI(pos->dy) <= 16) pos->dy = 0;
		
	Gamepad_SetAxis(axis, pos->dx / AXIS_SCALE, -pos->dy / AXIS_SCALE, delta);
}

static void ProcessTouchInput(int mods) {
	touchPosition touch;
	hidTouchRead(&touch);

	if (mods & KEY_TOUCH) {
		Input_AddTouch(0,    touch.px,      touch.py);
	} else if (hidKeysUp() & KEY_TOUCH) {
		Input_RemoveTouch(0, Pointers[0].x, Pointers[0].y);
	}
}

void Window_ProcessEvents(double delta) {
	hidScanInput();

	if (!aptMainLoop()) {
		Window_Main.Exists = false;
		Window_RequestClose();
		return;
	}
	
	u32 mods = hidKeysDown() | hidKeysHeld();
	HandleButtons(mods);

	ProcessTouchInput(mods);
	
	circlePosition hid_pos;
	hidCircleRead(&hid_pos);
	ProcessCircleInput(PAD_AXIS_RIGHT, &hid_pos, delta);

	if (irrst_result == 0) {
		circlePosition stk_pos;
		irrstScanInput();
		irrstCstickRead(&stk_pos);
		ProcessCircleInput(PAD_AXIS_LEFT, &stk_pos, delta);
	}
}

void Cursor_SetPosition(int x, int y) { } // Makes no sense for 3DS

void Window_EnableRawMouse(void)  { Input.RawMode = true;  }
void Window_DisableRawMouse(void) { Input.RawMode = false; }

void Window_UpdateRawMouse(void)  { }

/*########################################################################################################################*
*------------------------------------------------------Framebuffer--------------------------------------------------------*
*#########################################################################################################################*/
void Window_AllocFramebuffer(struct Bitmap* bmp) {
	bmp->scan0 = (BitmapCol*)Mem_Alloc(bmp->width * bmp->height, 4, "window pixels");
}

void Window_DrawFramebuffer(Rect2D r, struct Bitmap* bmp) {
	u16 width, height;
	gfxScreen_t screen = launcherTop ? GFX_TOP : GFX_BOTTOM;
	
	gfxSetDoubleBuffering(screen, false);
	u8* fb = gfxGetFramebuffer(screen, GFX_LEFT, &width, &height);
	// SRC y = 0 to 240
	// SRC x = 0 to 400
	// DST X = 0 to 240
	// DST Y = 0 to 400

	for (int y = r.y; y < r.y + r.Height; y++)
		for (int x = r.x; x < r.x + r.Width; x++)
	{
		BitmapCol color = Bitmap_GetPixel(bmp, x, y);
		int addr   = (width - 1 - y + x * width) * 3; // TODO -1 or not
		fb[addr+0] = BitmapCol_B(color);
		fb[addr+1] = BitmapCol_G(color);
		fb[addr+2] = BitmapCol_R(color);
	}
	// TODO gspWaitForVBlank();
	gfxFlushBuffers();
	//gfxSwapBuffers();
	// TODO: tearing??
	/*
	gfxSetDoubleBuffering(GFX_TOP, false);
	gfxScreenSwapBuffers(GFX_TOP, true);
	gfxSetDoubleBuffering(GFX_TOP, true);
	gfxScreenSwapBuffers(GFX_BOTTOM, true);
	*/
}

void Window_FreeFramebuffer(struct Bitmap* bmp) {
	Mem_Free(bmp->scan0);
}


/*########################################################################################################################*
*------------------------------------------------------Soft keyboard------------------------------------------------------*
*#########################################################################################################################*/
static void OnscreenTextChanged(const char* text) {
	char tmpBuffer[NATIVE_STR_LEN];
	cc_string tmp = String_FromArray(tmpBuffer);
	String_AppendUtf8(&tmp, text, String_Length(text));
    
	Event_RaiseString(&InputEvents.TextChanged, &tmp);
}

void Window_OpenKeyboard(struct OpenKeyboardArgs* args) {
	const char* btnText = args->type & KEYBOARD_FLAG_SEND ? "Send" : "Enter";
	char input[NATIVE_STR_LEN]  = { 0 };
	char output[NATIVE_STR_LEN] = { 0 };
	SwkbdState swkbd;
	String_EncodeUtf8(input, args->text);
	
	int mode = args->type & 0xFF;
	int type = (mode == KEYBOARD_TYPE_NUMBER || mode == KEYBOARD_TYPE_INTEGER) ? SWKBD_TYPE_NUMPAD : SWKBD_TYPE_WESTERN;
	
	swkbdInit(&swkbd, type, 3, -1);
	swkbdSetInitialText(&swkbd, input);
	swkbdSetHintText(&swkbd, args->placeholder);
	//swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
	//swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, btnText, true);
	swkbdSetButton(&swkbd, SWKBD_BUTTON_CONFIRM, btnText, true);
	
	if (type == KEYBOARD_TYPE_PASSWORD)
		swkbdSetPasswordMode(&swkbd, SWKBD_PASSWORD_HIDE_DELAY);
	if (args->multiline)
		swkbdSetFeatures(&swkbd, SWKBD_MULTILINE);
		
	// TODO filter callbacks and Window_Setkeyboardtext ??
	int btn = swkbdInputText(&swkbd, output, sizeof(output));
	if (btn != SWKBD_BUTTON_CONFIRM) return;
	OnscreenTextChanged(output);
}
void Window_SetKeyboardText(const cc_string* text) { }
void Window_CloseKeyboard(void) { /* TODO implement */ }


/*########################################################################################################################*
*-------------------------------------------------------Misc/Other--------------------------------------------------------*
*#########################################################################################################################*/
void Window_ShowDialog(const char* title, const char* msg) {
	/* TODO implement */
	Platform_LogConst(title);
	Platform_LogConst(msg);
}

cc_result Window_OpenFileDialog(const struct OpenFileDialogArgs* args) {
	return ERR_NOT_SUPPORTED;
}

cc_result Window_SaveFileDialog(const struct SaveFileDialogArgs* args) {
	return ERR_NOT_SUPPORTED;
}
#endif
