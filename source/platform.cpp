#include "platform.h"

#include "bootlog.h"
#include "log.h"

#include "imgui_citro3d.h"
#include "imgui_ctru.h"

#include <imgui.h>

#include <arpa/inet.h>
#include <malloc.h>
#include <unistd.h>

#include <cstdio>

namespace
{
/// The ImGui virtual screen: top screen occupies y [0,240), bottom y [240,480)
/// offset by x 40. The vendored citro3d renderer relies on this layout.
constexpr auto SCREEN_WIDTH  = 400.0f;
constexpr auto SCREEN_HEIGHT = 480.0f;

constexpr auto FB_SCALE  = 1.0f; // ANTI_ALIAS=0
constexpr auto FB_WIDTH  = SCREEN_WIDTH * FB_SCALE;
constexpr auto FB_HEIGHT = SCREEN_HEIGHT * FB_SCALE;

constexpr auto DISPLAY_TRANSFER_FLAGS =
    GX_TRANSFER_FLIP_VERT (0) | GX_TRANSFER_OUT_TILED (0) | GX_TRANSFER_RAW_COPY (0) |
    GX_TRANSFER_IN_FORMAT (GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT (GX_TRANSFER_FMT_RGB8) |
    GX_TRANSFER_SCALING (GX_TRANSFER_SCALE_NO);

/// SOC needs a 0x1000-aligned buffer whose size is a multiple of 0x1000.
constexpr auto SOC_ALIGN      = 0x1000u;
constexpr auto SOC_BUFFERSIZE = 0x80000u;
static_assert (SOC_BUFFERSIZE % SOC_ALIGN == 0);

u32 *s_socBuffer = nullptr;

C3D_RenderTarget *s_top      = nullptr;
C3D_RenderTarget *s_topRight = nullptr;
C3D_RenderTarget *s_bottom = nullptr;
void *s_depthStencil       = nullptr;

bool s_isNew3DS     = false;
bool s_backlightOff = false;
bool s_mcuHwc       = false;
bool s_socActive    = false;
bool s_gfxReady     = false;
bool s_gspLcd       = false;
bool s_imguiReady   = false;

u32 s_keysDown   = 0;
u32 s_keysHeld   = 0;
u32 s_keysUp     = 0;
u32 s_keysRepeat = 0;

/// Only the directions auto-repeat. Repeating A or Y would fire actions the
/// user never asked for just because they held the button a moment too long.
constexpr u32 REPEAT_KEYS = KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT | KEY_CPAD_UP |
                            KEY_CPAD_DOWN | KEY_CPAD_LEFT | KEY_CPAD_RIGHT;

/// How long a direction has to be held before it starts repeating, and how
/// fast it repeats once it does.
constexpr auto REPEAT_DELAY =
    std::chrono::duration_cast<platform::steady_clock::duration> (std::chrono::milliseconds (400));
constexpr auto REPEAT_INTERVAL =
    std::chrono::duration_cast<platform::steady_clock::duration> (std::chrono::milliseconds (70));

/// The directions held on the previous frame, and when the next pulse is due.
u32 s_repeatHeld = 0;
platform::steady_clock::time_point s_repeatDue;

/// \brief Fold auto-repeat pulses into s_keysRepeat for the current frame.
void updateRepeat ()
{
	auto const now  = platform::steady_clock::now ();
	auto const held = s_keysHeld & REPEAT_KEYS;

	s_keysRepeat = s_keysDown;

	// Any change to the set of held directions restarts the delay, so adding or
	// releasing one never inherits the previous one's repeat rhythm.
	if (held != s_repeatHeld)
	{
		s_repeatHeld = held;
		s_repeatDue  = now + REPEAT_DELAY;
		return;
	}

	if (!held || now < s_repeatDue)
		return;

	s_keysRepeat |= held;
	s_repeatDue = now + REPEAT_INTERVAL;
}
}

platform::steady_clock::time_point platform::steady_clock::now () noexcept
{
	return time_point (duration (svcGetSystemTick ()));
}

bool platform::init ()
{
	bootlog::step ("APT_CheckNew3DS");
	APT_CheckNew3DS (&s_isNew3DS);
	bootlog::note (s_isNew3DS ? "New 3DS" : "Old 3DS", true);

	// 804MHz + L2 cache. This goes through ptm:sysm, which not every
	// environment grants, so only ask for it where it does something.
	if (s_isNew3DS)
	{
		bootlog::step ("osSetSpeedupEnable");
		osSetSpeedupEnable (true);
	}

	// The app is useless asleep: transfers would stall on a closed lid.
	bootlog::step ("aptSetSleepAllowed");
	aptSetSleepAllowed (false);

	bootlog::step ("acInit");
	acInit ();
	bootlog::step ("ptmuInit");
	ptmuInit ();
	bootlog::step ("mcuHwcInit");
	s_mcuHwc = R_SUCCEEDED (mcuHwcInit ());
	bootlog::note ("mcu::HWC", s_mcuHwc);
	bootlog::step ("gspLcdInit");
	s_gspLcd = R_SUCCEEDED (gspLcdInit ());
	bootlog::note ("gsp::Lcd", s_gspLcd);

	// A failure here means no FTP, but the UI must still come up so the user
	// can read the error and quit cleanly rather than face a dead console.
	bootlog::step ("socInit");
	s_socBuffer = static_cast<u32 *> (::memalign (SOC_ALIGN, SOC_BUFFERSIZE));
	s_socActive = s_socBuffer && R_SUCCEEDED (socInit (s_socBuffer, SOC_BUFFERSIZE));
	bootlog::note ("soc:U", s_socActive);
	if (!s_socActive)
		logger::add (logger::ERROR, "Network unavailable (soc:U failed)");

	bootlog::step ("gfxInit");
	gfxInit (GSP_BGR8_OES, GSP_BGR8_OES, false);

	// Stereoscopy is enabled with a real right-eye target even though nothing
	// here needs depth. The vendored renderer reads the *physical* 3D slider
	// and offsets the left eye by it; with 3D off and no right target, a
	// console whose slider happens to be up would draw the top screen shifted
	// sideways. Emulators report the slider as 0, so this only bites on
	// hardware. Matching ftpd's configuration is the tested way out.
	gfxSet3D (true);

	bootlog::step ("C3D_Init");
	C3D_Init (4 * C3D_DEFAULT_CMDBUF_SIZE);
	s_gfxReady = true;

	bootlog::step ("render targets");
	s_top = C3D_RenderTargetCreate (FB_HEIGHT * 0.5f, FB_WIDTH, GPU_RB_RGBA8, -1);
	C3D_RenderTargetSetOutput (s_top, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

	s_topRight = C3D_RenderTargetCreate (FB_HEIGHT * 0.5f, FB_WIDTH, GPU_RB_RGBA8, -1);
	C3D_RenderTargetSetOutput (s_topRight, GFX_TOP, GFX_RIGHT, DISPLAY_TRANSFER_FLAGS);

	s_bottom = C3D_RenderTargetCreate (FB_HEIGHT * 0.5f, FB_WIDTH * 0.8f, GPU_RB_RGBA8, -1);
	C3D_RenderTargetSetOutput (s_bottom, GFX_BOTTOM, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

	{
		auto const size =
		    C3D_CalcDepthBufSize (FB_HEIGHT * 0.5f, FB_WIDTH, GPU_RB_DEPTH24_STENCIL8);
		bootlog::step ("vramAlloc depth");
		s_depthStencil = vramAlloc (size);
		bootlog::note ("depth buffer", s_depthStencil != nullptr);
		C3D_FrameBufDepth (&s_top->frameBuf, s_depthStencil, GPU_RB_DEPTH24_STENCIL8);
		C3D_FrameBufDepth (&s_topRight->frameBuf, s_depthStencil, GPU_RB_DEPTH24_STENCIL8);
		C3D_FrameBufDepth (&s_bottom->frameBuf, s_depthStencil, GPU_RB_DEPTH24_STENCIL8);
	}

	bootlog::step ("ImGui::CreateContext");
	ImGui::CreateContext ();

	bootlog::step ("imgui::ctru::init");
	if (!imgui::ctru::init ())
	{
		bootlog::note ("imgui::ctru::init", false);
		return false;
	}

	// This is where the glyph atlas is built from the console's shared system
	// font, the heaviest and most environment-dependent step in start-up.
	bootlog::step ("imgui::citro3d::init (system font atlas)");
	imgui::citro3d::init ();
	s_imguiReady = true;

	auto &io = ImGui::GetIO ();
	// No .ini: there is nowhere sensible to persist window layout, and every
	// window here is positioned explicitly anyway.
	io.IniFilename = nullptr;
	// We drive navigation ourselves (per-list cursor) to match the exact
	// A/B/X/Y/L/R/SELECT semantics the app needs.
	io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;

	ImGui::GetStyle ().ScaleAllSizes (0.5f);

	bootlog::step ("platform ready");
	return true;
}

bool platform::loop ()
{
	if (!aptMainLoop ())
		return false;

	hidScanInput ();

	// Fully qualified: see the note in platform.h about libctru's hid.h macros.
	s_keysDown = ::hidKeysDown ();
	s_keysHeld = ::hidKeysHeld ();
	s_keysUp   = ::hidKeysUp ();

	if (s_backlightOff)
	{
		if (s_keysDown)
		{
			backlightOn ();
			// swallow the wake press so it does not also act on the UI
			s_keysDown = s_keysHeld = s_keysUp = 0;
		}
		else
		{
			// keep the loop (and the transfer worker) alive without burning
			// the CPU or touching the GPU
			gspWaitForVBlank ();
			return true;
		}
	}

	updateRepeat ();

	auto &io                   = ImGui::GetIO ();
	io.DisplaySize             = ImVec2 (SCREEN_WIDTH, SCREEN_HEIGHT);
	io.DisplayFramebufferScale = ImVec2 (FB_SCALE, FB_SCALE);

	imgui::ctru::newFrame ();
	ImGui::NewFrame ();

	return true;
}

void platform::render ()
{
	if (s_backlightOff)
		return;

	ImGui::Render ();

	C3D_FrameBegin (C3D_FRAME_SYNCDRAW);
	imgui::citro3d::render (s_top, s_topRight, s_bottom);
	C3D_FrameEnd (0);

	// One breadcrumb from the render path: it separates "hung during start-up"
	// from "hung on the first frame" in boot.log.
	static bool first = true;
	if (first)
	{
		first = false;
		bootlog::step ("first frame presented");
		bootlog::end ();
	}
}

void platform::exit ()
{
	// init() can bail out part-way (no network, no GPU memory), and main calls
	// exit() on that path too. Only tear down what actually came up.
	if (s_imguiReady)
	{
		imgui::citro3d::exit ();
		ImGui::DestroyContext ();
	}

	if (s_depthStencil)
		vramFree (s_depthStencil);

	if (s_gfxReady)
	{
		C3D_Fini ();
		gfxExit ();
	}

	backlightOn ();
	if (s_gspLcd)
		gspLcdExit ();

	if (s_socActive)
		socExit ();
	if (s_socBuffer)
		::free (s_socBuffer);

	if (s_mcuHwc)
		mcuHwcExit ();
	ptmuExit ();
	acExit ();

	// restore what we changed console-wide
	aptSetSleepAllowed (true);
	if (s_isNew3DS)
		osSetSpeedupEnable (false);
}

std::uint32_t platform::buttonsDown ()
{
	return s_keysDown;
}

std::uint32_t platform::buttonsRepeat ()
{
	return s_keysRepeat;
}

std::uint32_t platform::buttonsHeld ()
{
	return s_keysHeld;
}

std::uint32_t platform::buttonsUp ()
{
	return s_keysUp;
}

bool platform::isNew3DS ()
{
	return s_isNew3DS;
}

void platform::backlightOff ()
{
	if (s_backlightOff)
		return;

	if (!s_gspLcd)
		return;

	GSPLCD_PowerOffBacklight (GSPLCD_SCREEN_BOTH);
	s_backlightOff = true;
}

void platform::backlightOn ()
{
	if (!s_backlightOff)
		return;

	GSPLCD_PowerOnBacklight (GSPLCD_SCREEN_BOTH);
	s_backlightOff = false;
}

bool platform::backlightIsOff ()
{
	return s_backlightOff;
}

namespace
{
/// Battery state comes from IPC services, so it is polled on a timer rather
/// than every frame. It changes on the order of minutes anyway.
constexpr std::uint64_t BATTERY_POLL_TICKS = SYSCLOCK_ARM11 * 5; // 5 seconds

int s_batteryPercent      = -1;
bool s_batteryCharging    = false;
std::uint64_t s_batteryAt = 0;

void pollBattery ()
{
	auto const now = svcGetSystemTick ();
	if (s_batteryAt && now - s_batteryAt < BATTERY_POLL_TICKS)
		return;

	s_batteryAt = now;

	if (s_mcuHwc)
	{
		u8 percent;
		if (R_SUCCEEDED (MCUHWC_GetBatteryLevel (&percent)))
			s_batteryPercent = percent;
		else
		{
			// mcu::HWC is not always reachable (some emulators, some CFW
			// setups). Stop asking and fall back to PTMU's coarse level.
			s_mcuHwc = false;
		}
	}

	if (!s_mcuHwc)
	{
		u8 level;
		s_batteryPercent = R_SUCCEEDED (PTMU_GetBatteryLevel (&level)) ? level * 20 : -1;
	}

	u8 charging;
	s_batteryCharging =
	    R_SUCCEEDED (PTMU_GetBatteryChargeState (&charging)) && charging != 0;
}
}

int platform::batteryPercent ()
{
	pollBattery ();
	return s_batteryPercent;
}

bool platform::batteryCharging ()
{
	pollBattery ();
	return s_batteryCharging;
}

int platform::wifiStrength ()
{
	return osGetWifiStrength ();
}

char const *platform::localAddress ()
{
	// gethostid() is an IPC round trip to the SOC service. The status bar asks
	// for it every frame, so cache it the way the battery is cached.
	static char buffer[32]      = "0.0.0.0";
	static std::uint64_t polled = 0;

	if (!s_socActive)
		return buffer;

	auto const now = svcGetSystemTick ();
	if (polled && now - polled < SYSCLOCK_ARM11 * 5)
		return buffer;

	polled = now;

	in_addr addr;
	addr.s_addr = gethostid ();
	std::snprintf (buffer, sizeof (buffer), "%s", inet_ntoa (addr));

	return buffer;
}
