#pragma once

#include <3ds.h>

#include <chrono>
#include <cstdint>
#include <ratio>

namespace platform
{
/// \brief Monotonic clock backed by the ARM11 system tick counter.
/// Required by the vendored imgui_ctru backend, and used for transfer ETAs.
struct steady_clock
{
	using rep                       = std::uint64_t;
	using period                    = std::ratio<1, SYSCLOCK_ARM11>;
	using duration                  = std::chrono::duration<rep, period>;
	using time_point                = std::chrono::time_point<steady_clock>;
	constexpr static bool is_steady = true;

	static time_point now () noexcept;
};

/// \brief Bring up every service the app needs. Returns false if something
/// essential (gfx, soc, imgui) could not be initialized.
bool init ();

/// \brief Begin a frame: scan input and start the ImGui frame.
/// \returns false when the app has been asked to quit (HOME menu close, etc.)
bool loop ();

/// \brief End a frame: render ImGui to both screens.
void render ();

/// Button state for this frame. These return 0 on the frame that woke the
/// screen back up, so the wake press never also triggers an action.
///
/// Deliberately NOT called keysDown/keysHeld/keysUp: libctru's hid.h defines
/// those exact names as compatibility macros for hidKeysDown/Held/Up, so the
/// preprocessor would rename these to platform::hidKeysDown(), and unqualified
/// calls to hidKeysDown() inside namespace platform would then resolve to them
/// instead of to libctru — silently turning the reads below into no-ops.
std::uint32_t buttonsDown ();
std::uint32_t buttonsHeld ();
std::uint32_t buttonsUp ();

/// \brief buttonsDown() plus auto-repeat pulses for the d-pad and circle pad
/// once a direction has been held down. This is what the menus and lists read,
/// so holding a direction scrolls instead of needing one press per row.
std::uint32_t buttonsRepeat ();

/// \brief Tear everything down, restoring settings we changed globally.
void exit ();

/// \brief Whether we are running on a New 3DS (or better).
bool isNew3DS ();

/// \brief Turn both LCD backlights off. Transfers and input keep running.
void backlightOff ();

/// \brief Turn both LCD backlights back on.
void backlightOn ();

/// \brief Whether the backlights are currently off.
bool backlightIsOff ();

/// \brief Battery charge in percent, or -1 if unavailable.
int batteryPercent ();

/// \brief Whether the console is plugged in and charging.
bool batteryCharging ();

/// \brief Wifi signal strength, 0 (none) to 3 (full).
int wifiStrength ();

/// \brief Our own IPv4 address as a dotted string, or "0.0.0.0" if not online.
char const *localAddress ();
}
