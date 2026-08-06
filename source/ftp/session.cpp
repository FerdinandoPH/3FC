#include "session.h"

#include "bootlog.h"
#include "log.h"

#include <3ds.h>

#include <deque>

namespace
{
/// The worker runs on the application core (-2 = the one from the exheader).
///
/// It used to ask for core 1, the *system* core, via APT_SetAppCpuTimeLimit.
/// That is a well-known way to hang a console — it competes with the system
/// modules — and the gain was speculative anyway: this thread spends nearly all
/// its time blocked in poll(), so the scheduler runs the UI thread regardless
/// of which core it nominally lives on. Build with -DWORKER_ON_SYSTEM_CORE to
/// go back to the old behaviour.
#ifdef WORKER_ON_SYSTEM_CORE
constexpr int WORKER_CORE = 1;
#else
constexpr int WORKER_CORE = -2;
#endif

constexpr std::size_t STACK_SIZE = 64 * 1024;

/// \brief One step *above* the UI thread.
///
/// It used to be one step below, on the reasoning that the UI should win a tie.
/// Measured on hardware, that cost 2.4 frames of latency per syscall: the
/// worker only got the core back once the 60 Hz UI thread blocked on vblank
/// again, so an 8 MB download spent 157 s waiting to be rescheduled rather than
/// waiting on the network or the card.
///
/// Above the UI is safe here because this thread is I/O bound — it is blocked
/// in recv(), in the SD driver or on its wake event essentially all the time —
/// so it yields the core constantly and cannot starve the frame loop.
int workerPriority ()
{
	s32 priority = 0x30;
	svcGetThreadPriority (&priority, CUR_THREAD_HANDLE);

	// 0x18 is as high as a userland app is allowed to ask for.
	return priority > 0x18 ? priority - 1 : priority;
}

LightLock s_lock;
LightEvent s_wake;

Thread s_thread    = nullptr;
bool s_running     = false;
bool s_taskRunning = false;

std::deque<session::Task> s_tasks;

ftp::Client s_client;

session::State s_state = session::DISCONNECTED;
std::string s_error;
std::string s_description;

std::string s_listPath;
fs::Listing s_listing;
int s_listPending      = 0;
unsigned s_listGeneration = 0;

struct Guard
{
	Guard ()
	{
		LightLock_Lock (&s_lock);
	}
	~Guard ()
	{
		LightLock_Unlock (&s_lock);
	}
};

void workerMain (void *)
{
	while (true)
	{
		session::Task task;

		{
			Guard guard;

			if (!s_running)
				break;

			if (!s_tasks.empty ())
			{
				task = std::move (s_tasks.front ());
				s_tasks.pop_front ();
				s_taskRunning = true;
			}
		}

		if (!task)
		{
			LightEvent_Wait (&s_wake);
			LightEvent_Clear (&s_wake);
			continue;
		}

		task (s_client);

		Guard guard;
		s_taskRunning = false;

		// Nothing else notices a link that dropped mid-session: the UI would go
		// on offering a browser backed by a connection that no longer exists,
		// and every action would fail one timeout at a time. Checking here
		// catches it whichever task happened to be the one that hit it.
		if (s_state == session::CONNECTED && !s_client.alive ())
		{
			s_state = session::LOST;
			s_error = "Connection lost";

			// The cached listing describes a server we can no longer talk to.
			s_listing.clear ();
			s_listPath.clear ();
		}
	}

	s_client.disconnect ();
}
}

bool session::init ()
{
	LightLock_Init (&s_lock);
	LightEvent_Init (&s_wake, RESET_STICKY);

	s_running = true;

#ifdef WORKER_ON_SYSTEM_CORE
	// Creating a thread on the system core is only allowed once the app has
	// been granted a share of it.
	bootlog::step ("APT_SetAppCpuTimeLimit");
	APT_SetAppCpuTimeLimit (30);
#endif

	bootlog::step ("threadCreate (network worker)");
	s_thread = threadCreate (workerMain, nullptr, STACK_SIZE, workerPriority (), WORKER_CORE, false);

	if (!s_thread)
	{
		bootlog::note ("network worker", false);
		logger::add (logger::ERROR, "Could not start the network thread");
		s_running = false;
		return false;
	}

	bootlog::note ("network worker", true);
	return true;
}

void session::exit ()
{
	if (!s_thread)
		return;

	{
		Guard guard;
		s_running = false;
		s_tasks.clear ();
	}

	LightEvent_Signal (&s_wake);
	threadJoin (s_thread, U64_MAX);
	threadFree (s_thread);
	s_thread = nullptr;
}

void session::post (Task task)
{
	{
		Guard guard;
		s_tasks.push_back (std::move (task));
	}

	LightEvent_Signal (&s_wake);
}

bool session::busy ()
{
	Guard guard;
	return s_taskRunning || !s_tasks.empty ();
}

session::State session::state ()
{
	Guard guard;
	return s_state;
}

std::string session::error ()
{
	Guard guard;
	return s_error;
}

std::string session::description ()
{
	Guard guard;
	return s_description;
}

void session::connect (config::Slot const &slot)
{
	auto const host     = slot.host;
	auto const port     = slot.port;
	auto const user     = slot.effectiveUser ();
	auto const password = slot.effectivePassword ();
	auto const label    = slot.label ();

	{
		Guard guard;
		s_state = CONNECTING;
		s_error.clear ();
		s_description = label;
		s_listing.clear ();
		s_listPath.clear ();
	}

	post ([host, port, user, password] (ftp::Client &client) {
		auto const ok = client.connect (host, port, user, password);

		Guard guard;
		s_state = ok ? CONNECTED : FAILED;
		if (!ok)
			s_error = "Could not connect to " + host;
	});
}

void session::disconnect ()
{
	{
		// Clear the state up front: the caller usually reacts to FAILED, and
		// leaving it set until the worker gets around to the task would make
		// that reaction fire on every frame in between.
		Guard guard;
		s_state = DISCONNECTED;
	}

	post ([] (ftp::Client &client) {
		client.disconnect ();

		Guard guard;
		s_state = DISCONNECTED;
		s_listing.clear ();
		s_listPath.clear ();
		s_description.clear ();
	});
}

void session::requestList (std::string path)
{
	{
		Guard guard;
		++s_listPending;
	}

	post ([path] (ftp::Client &client) {
		fs::Listing listing;
		auto const ok = client.list (path, listing);

		Guard guard;
		--s_listPending;

		if (!ok)
		{
			s_error = "Could not list " + path;
			listing.clear ();
		}

		// Published either way: on failure the browser must end up showing an
		// empty folder for the path it asked about, not the previous one's rows.
		s_listPath = path;
		s_listing  = std::move (listing);
		++s_listGeneration;
	});
}

void session::listing (std::string &path, fs::Listing &out)
{
	Guard guard;
	path = s_listPath;
	out  = s_listing;
}

bool session::listPending ()
{
	Guard guard;
	return s_listPending > 0;
}

unsigned session::listGeneration ()
{
	Guard guard;
	return s_listGeneration;
}
