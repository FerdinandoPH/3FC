#include "app.h"
#include "bootlog.h"
#include "config/config.h"
#include "ftp/session.h"
#include "transfer/queue.h"
#include "i18n/lang.h"
#include "log.h"
#include "platform.h"

int main (int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	// Written before anything else can hang: on hardware there is no console
	// and no debugger, so this file is the only way to see how far start-up got.
	bootlog::begin ();

	bootlog::step ("logger::init");
	logger::init ();
	bootlog::step ("i18n::init");
	i18n::init ();
	bootlog::step ("config::load");
	config::load ();

	bootlog::step ("platform::init");

	if (!platform::init ())
	{
		platform::exit ();
		return 1;
	}

	bootlog::step ("transfer::init");
	transfer::init ();

	// Without the network worker nothing that touches FTP can ever complete,
	// and the UI would sit on "connecting" forever with no way out. Say so.
	if (!session::init ())
		app::state ().confirm.notify (TR (NO_WORKER));

#ifdef AUTOCONNECT
	// Build with EXTRA_DEFINES=-DAUTOCONNECT to connect to slot 0 on start-up
	// and immediately download the server's root into a scratch folder.
	// Emulators do not deliver injected button presses, so this is the only way
	// to exercise the browser and the transfer queue there.
	session::connect (config::slot (0));

	{
		transfer::Request download;
		download.direction = transfer::DOWNLOAD;
		download.source    = "/";
		download.destDir   = "sdmc:/3ds/3FC/test";
		transfer::enqueue (download);

		// ...and push it straight back, which exercises the upload path and
		// proves the round trip preserves names and bytes.
		transfer::Request upload;
		upload.direction = transfer::UPLOAD;
		upload.source    = "sdmc:/3ds/3FC/test";
		upload.destDir   = "/";
		transfer::enqueue (upload);
	}

	app::state ().bottomTab = app::TAB_PROGRESS;
	app::state ().focus     = app::FOCUS_BOTTOM;
#endif

	logger::add (logger::INFO, platform::isNew3DS () ? "New 3DS detected" : "Old 3DS detected");
	bootlog::step ("entering main loop");

	while (platform::loop ())
	{
		// With the backlight off we deliberately build no UI at all; loop()
		// has already paced the frame on vblank.
		if (!platform::backlightIsOff ())
			app::frame ();

		platform::render ();

		if (app::state ().quit)
			break;
	}

	// Quitting cancels whatever is in flight, which the user confirmed in the
	// START menu.
	transfer::cancelAll ();
	session::exit ();
	platform::exit ();
	logger::exit ();

	return 0;
}
