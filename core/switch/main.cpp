
#include <assert.h>
#include <poll.h>
#include <fcntl.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/time.h>
#include "hw/sh4/dyna/blockmanager.h"
#include <unistd.h>
#include "hw/maple/maple_cfg.h"
#include "stdclass.h"
#include <switch.h>

#include <EGL/egl.h>    // EGL library
#include <GLES2/gl2.h>  // OpenGL ES 2.0 library

#define TRACE printf

void dc_stop(void);
#define key_CONT_B            (1 << 1)
#define key_CONT_A            (1 << 2)
#define key_CONT_START        (1 << 3)
#define key_CONT_DPAD_UP      (1 << 4)
#define key_CONT_DPAD_DOWN    (1 << 5)
#define key_CONT_DPAD_LEFT    (1 << 6)
#define key_CONT_DPAD_RIGHT   (1 << 7)
#define key_CONT_Y            (1 << 9)
#define key_CONT_X            (1 << 10)

static int s_nxlinkSock = -1;

static void initNxLink()
{
	if (R_FAILED(socketInitializeDefault()))
		return;

	s_nxlinkSock = nxlinkStdio();
	if (s_nxlinkSock >= 0)
		printf("printf output now goes to nxlink server");
	else
		socketExit();
}

static void deinitNxLink()
{
	if (s_nxlinkSock >= 0)
	{
		close(s_nxlinkSock);
		socketExit();
		s_nxlinkSock = -1;
	}
}

extern "C" void userAppInit()
{
	initNxLink();
}

extern "C" void userAppExit()
{
	deinitNxLink();
}

static void setMesaConfig()
{
    // Uncomment below to disable error checking and save CPU time (useful for production):
    //setenv("MESA_NO_ERROR", "1", 1);

    // Uncomment below to enable Mesa logging:
    //setenv("EGL_LOG_LEVEL", "debug", 1);
    //setenv("MESA_VERBOSE", "all", 1);
    //setenv("NOUVEAU_MESA_DEBUG", "1", 1);

    // Uncomment below to enable shader debugging in Nouveau:
    //setenv("NV50_PROG_OPTIMIZE", "0", 1);
    //setenv("NV50_PROG_DEBUG", "1", 1);
    //setenv("NV50_PROG_CHIPSET", "0x120", 1);
}

u16 kcode[4];
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];

int reicast_init(int argc, char* argv[]);
void *rend_thread(void *);
void dc_term();

void* libPvr_GetRenderTarget() {
	return nwindowGetDefault();
}

void *libPvr_GetRenderSurface() {
	return EGL_DEFAULT_DISPLAY;
}

extern "C" int main(int argc, wchar* argv[])
{
    // Set mesa configuration (useful for debugging)
    setMesaConfig();

    std::string homedir = "/reicast";
    set_user_config_dir(homedir);
    set_user_data_dir(homedir);

    printf("Config dir is: %s\n", get_writable_config_path("/").c_str());
    printf("Data dir is:   %s\n", get_writable_data_path("/").c_str());

    settings.profile.run_counts=0;

    reicast_init(argc,argv);

	rend_thread(0);

	dc_term();

    return 0;
}

void os_DoEvents() {

}


u32  os_Push(void*, u32, bool) {
    return 1;
}

void os_SetWindowText(const char* t) {
    puts(t);
}

void os_CreateWindow() {

}

void os_SetupInput() {
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
	mcfg_CreateDevices();
#endif
}

void UpdateInputState(u32 port) {
    hidScanInput();
    u32 kDown = hidKeysHeld(CONTROLLER_P1_AUTO);
	kcode[port]=0xFFFF;
    if (kDown & KEY_B)
		kcode[port]&=~key_CONT_A;
    if (kDown & KEY_A)
		kcode[port]&=~key_CONT_B;
    if (kDown & KEY_X)
		kcode[port]&=~key_CONT_Y;
    if (kDown & KEY_Y)
		kcode[port]&=~key_CONT_X;

    if (kDown & KEY_DLEFT)
		kcode[port]&=~key_CONT_DPAD_LEFT;
    if (kDown & KEY_DRIGHT)
		kcode[port]&=~key_CONT_DPAD_RIGHT;
    if (kDown & KEY_DUP)
		kcode[port]&=~key_CONT_DPAD_UP;
    if (kDown & KEY_DDOWN)
		kcode[port]&=~key_CONT_DPAD_DOWN;

    if (kDown & KEY_PLUS)
		kcode[port]&=~key_CONT_START;
    if (kDown & KEY_MINUS)
		dc_stop();

	lt[port]=(kDown & (KEY_ZL | KEY_L)) ? 255 : 0;
	rt[port]=(kDown & (KEY_ZR | KEY_R)) ? 255 : 0;

	JoystickPosition pos_left;
    hidJoystickRead(&pos_left, CONTROLLER_P1_AUTO, JOYSTICK_LEFT);
	joyx[port] = pos_left.dx / 257;
	joyy[port] = -pos_left.dy / 257;
}

void UpdateVibration(u32 port, u32 value) {
}

int get_mic_data(u8* buffer) {
}

void os_DebugBreak() { exit(-1); }

double os_GetSeconds() {
	timeval a;
	gettimeofday (&a,0);
	static u64 tvs_base=a.tv_sec;
	return a.tv_sec-tvs_base+a.tv_usec/1000000.0;
}

int push_vmu_screen(u8* buffer) { return 0; }

void VArray2::LockRegion(u32 offset,u32 size) {}
void VArray2::UnLockRegion(u32 offset,u32 size) {}

alignas(16) u8 __nx_exception_stack[0x1000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

void __libnx_exception_handler(ThreadExceptionDump *ctx)
{
    int i;

    printf("error_desc: 0x%x\n", ctx->error_desc);//You can also parse this with ThreadExceptionDesc.
    //This assumes AArch64, however you can also use threadExceptionIsAArch64().
    for(i=0; i<29; i++)printf("[X%d]: 0x%lx\n", i, ctx->cpu_gprs[i].x);
    printf("fp: 0x%lx\n", ctx->fp.x);
    printf("lr: 0x%lx\n", ctx->lr.x);
    printf("sp: 0x%lx\n", ctx->sp.x);
    printf("pc: 0x%lx\n", ctx->pc.x);

    //You could print fpu_gprs if you want.

    printf("pstate: 0x%x\n", ctx->pstate);
    printf("afsr0: 0x%x\n", ctx->afsr0);
    printf("afsr1: 0x%x\n", ctx->afsr1);
    printf("esr: 0x%x\n", ctx->esr);

    printf("far: 0x%lx\n", ctx->far.x);

}



