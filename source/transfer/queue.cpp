#include "queue.h"

#include "fs/localFs.h"
#include "ftp/session.h"
#include "log.h"
#include "platform.h"

#include <3ds.h>

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <deque>

namespace
{
/// Transfer chunk. The single biggest lever on real throughput over the 3DS's
/// wifi; smaller values spend all their time in syscalls. New 3DS has both the
/// spare RAM and the memory bandwidth to make a larger one worthwhile, while
/// Old 3DS has a much tighter heap under the Homebrew Launcher.
std::size_t chunkSize ()
{
	return platform::isNew3DS () ? 128 * 1024 : 64 * 1024;
}

constexpr int DATA_TIMEOUT_MS = 30000;

/// \brief RAII file on the SD card, on raw descriptors rather than stdio.
///
/// devkitARM's BUFSIZ is 1024, and newlib sizes a FILE's buffer from it, so
/// fwrite/fread chop every call into 1 KB pieces no matter how big a chunk we
/// hand them. Each of those pieces is a round trip to the FS service, which is
/// what makes SD access on this console expensive. Writing the whole chunk in
/// one call turns a hundred-odd of them into one.
class File
{
public:
	~File ()
	{
		close ();
	}

	File ()                         = default;
	File (File const &)             = delete;
	File &operator= (File const &)  = delete;

	bool open (std::string const &path, int const flags)
	{
		close ();
		m_fd = ::open (path.c_str (), flags, 0666);
		return m_fd >= 0;
	}

	void close ()
	{
		if (m_fd >= 0)
		{
			::close (m_fd);
			m_fd = -1;
		}
	}

	/// \returns bytes read, 0 at end of file, -1 on error. A short read is not
	///          end of file, so the caller keeps going.
	std::ptrdiff_t read (void *const buffer, std::size_t const size)
	{
		return ::read (m_fd, buffer, size);
	}

	/// \returns true only if the whole buffer made it to the card
	bool write (void const *const buffer, std::size_t size)
	{
		auto const *data = static_cast<std::uint8_t const *> (buffer);

		while (size > 0)
		{
			auto const wrote = ::write (m_fd, data, size);
			if (wrote <= 0)
				return false;

			data += wrote;
			size -= static_cast<std::size_t> (wrote);
		}

		return true;
	}

private:
	int m_fd = -1;
};

LightLock s_lock;

std::vector<transfer::Job> s_jobs;
std::deque<transfer::Request> s_requests;

unsigned s_nextId    = 1;
bool s_draining      = false;
bool s_expanding     = false;
unsigned s_activeId  = 0;
bool s_cancelActive  = false;

/// Stops the worker taking on anything new. The job already running is left
/// alone: this exists so a "cancel the pending ones?" prompt still means the
/// same thing by the time it is answered.
bool s_paused = false;

/// Exponential moving average of the active job's throughput.
double s_rate                              = 0.0;
platform::steady_clock::time_point s_rateAt;
/// Bytes seen since s_rateAt. Chunks arrive far faster than the sampling
/// window, so they have to pile up here rather than be measured one by one.
std::uint64_t s_rateBytes = 0;
bool s_rateValid          = false;

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

transfer::Job *findJob (unsigned const id)
{
	auto const it = std::find_if (s_jobs.begin (), s_jobs.end (), [id] (transfer::Job const &job) {
		return job.id == id;
	});

	return it == s_jobs.end () ? nullptr : &*it;
}

/// \brief Called from the worker between chunks.
bool cancelRequested (void *)
{
	Guard guard;
	return s_cancelActive;
}

void resetRate ()
{
	Guard guard;
	s_rate      = 0.0;
	s_rateBytes = 0;
	s_rateValid = false;
}

/// \brief Fold a chunk into the throughput estimate and publish progress.
void reportProgress (unsigned const id, std::uint64_t const done, std::size_t const delta)
{
	auto const now = platform::steady_clock::now ();

	Guard guard;

	if (auto *const job = findJob (id))
		job->done = done;

	if (!s_rateValid)
	{
		s_rateAt    = now;
		s_rateBytes = 0;
		s_rateValid = true;
		return;
	}

	s_rateBytes += delta;

	auto const seconds = std::chrono::duration<double> (now - s_rateAt).count ();
	if (seconds < 0.25)
		return; // too short a window to say anything useful

	// Every chunk since the last sample, not just this one: dividing a single
	// chunk by a window that held a dozen of them under-reports the rate by
	// exactly that factor.
	auto const instant = static_cast<double> (s_rateBytes) / seconds;

	s_rateAt    = now;
	s_rateBytes = 0;

	// ~1s time constant, so the ETA settles quickly but does not jitter.
	s_rate = s_rate > 0.0 ? s_rate * 0.75 + instant * 0.25 : instant;
}

/// How deep a tree we are willing to walk.
///
/// A server that lists a directory as its own child — a symlink loop, or simply
/// a buggy server — would otherwise recurse until the worker's 64 KB stack runs
/// out, which on this console is an instant freeze with no message.
constexpr int MAX_DEPTH = 32;

/// \brief Recursively walk \a source, appending jobs that recreate it under
/// \a destDir. Runs on the worker: remote walks need listings.
///
/// \returns false if the tree could not be walked in full. The caller throws
///          the whole expansion away in that case: half a tree is worse than
///          none, because the missing half is invisible and the jobs that did
///          get queued would recreate a structure that does not match the
///          source.
bool expand (ftp::Client &client,
    transfer::Direction const direction,
    std::string const &source,
    std::string const &destDir,
    transfer::Overwrite const overwrite,
    int const depth,
    std::vector<transfer::Job> &out)
{
	auto const remoteSource = direction == transfer::DOWNLOAD;
	auto const name         = fs::basename (source);
	auto const dest         = fs::join (destDir, name);

	if (depth > MAX_DEPTH)
	{
		logger::printf (logger::ERROR, "Too many nested folders under %s", source.c_str ());
		return false;
	}

	bool isDirectory;
	std::uint64_t size = 0;

	if (remoteSource)
	{
		// Asking the server what this is. A failure to *reach* the server is
		// not an answer: treating it as "directory" is what used to create
		// folders named after files whenever the link hiccuped.
		switch (client.probe (source, size))
		{
		case ftp::Client::PROBE_FILE:
			isDirectory = false;
			break;

		case ftp::Client::PROBE_DIRECTORY:
			isDirectory = true;
			break;

		default:
			logger::printf (logger::ERROR, "Could not inspect %s", source.c_str ());
			return false;
		}
	}
	else
	{
		if (!fs::local::exists (source))
		{
			logger::printf (logger::ERROR, "%s is gone", source.c_str ());
			return false;
		}

		isDirectory = fs::local::isDirectory (source);
		if (!isDirectory)
			fs::local::size (source, size);
	}

	if (!isDirectory)
	{
		// "No to all" on the overwrite prompt: existing destinations are
		// skipped rather than queued and then refused.
		if (overwrite == transfer::OVERWRITE_NO)
		{
			auto const present = direction == transfer::UPLOAD ? client.exists (dest)
			                                                   : fs::local::exists (dest);

			// A remote check that failed because the session died says nothing
			// about whether the file is there, so do not act on it.
			if (direction == transfer::UPLOAD && !client.alive ())
				return false;

			if (present)
				return true;
		}

		transfer::Job job;
		job.direction = direction;
		job.source    = source;
		job.dest      = dest;
		job.name      = name;
		job.size      = size;
		out.push_back (std::move (job));
		return true;
	}

	// The directory itself comes first so children always have somewhere to go.
	transfer::Job dirJob;
	dirJob.direction = direction;
	dirJob.source    = source;
	dirJob.dest      = dest;
	dirJob.name      = name + "/";
	dirJob.directory = true;
	out.push_back (std::move (dirJob));

	fs::Listing listing;
	auto const ok =
	    remoteSource ? client.list (source, listing) : fs::local::list (source, listing);

	if (!ok)
	{
		logger::printf (logger::ERROR, "Could not list %s", source.c_str ());
		return false;
	}

	for (auto const &entry : listing)
	{
		if (entry.type == fs::PARENT || entry.name == "." || entry.name == "..")
			continue;

		if (!expand (client,
		        direction,
		        fs::join (source, entry.name),
		        dest,
		        overwrite,
		        depth + 1,
		        out))
			return false;
	}

	return true;
}

bool runDirectory (ftp::Client &client, transfer::Job const &job)
{
	if (job.direction == transfer::DOWNLOAD)
		return fs::local::makeDirectories (job.dest);

	if (job.direction == transfer::UPLOAD)
	{
		// An existing directory is not an error: recreating a tree that is
		// partly there is normal. A session that died trying is, though —
		// otherwise the whole tree "succeeds" without anything being created.
		client.makeDirectory (job.dest);
		return client.alive ();
	}

	return fs::local::makeDirectories (job.dest);
}

/// \brief Where a transfer's time actually went.
///
/// There is no profiler on this console, and "it feels slow" does not say
/// whether the socket or the SD card is the one holding things up. One line per
/// job in the console answers that in a single run on hardware.
struct Timing
{
	platform::steady_clock::duration net{};
	platform::steady_clock::duration disk{};
	unsigned netCalls  = 0;
	unsigned diskCalls = 0;
};

double toSeconds (platform::steady_clock::duration const d)
{
	return std::chrono::duration<double> (d).count ();
}

void logTiming (char const *const what,
    std::string const &name,
    std::uint64_t const bytes,
    Timing const &timing,
    platform::steady_clock::duration const total)
{
	auto const seconds = toSeconds (total);
	if (seconds <= 0.0 || bytes == 0)
		return;

	logger::printf (logger::INFO,
	    "%s %s: %.0f KB/s | net %.2fs/%u calls | sd %.2fs/%u calls | total %.2fs",
	    what,
	    name.c_str (),
	    bytes / 1024.0 / seconds,
	    toSeconds (timing.net),
	    timing.netCalls,
	    toSeconds (timing.disk),
	    timing.diskCalls,
	    seconds);
}

bool runDownload (ftp::Client &client, transfer::Job const &job)
{
	if (!fs::local::makeDirectories (fs::parent (job.dest)))
	{
		logger::printf (logger::ERROR, "Cannot create %s", fs::parent (job.dest).c_str ());
		return false;
	}

	// Downloads land in a scratch file and are moved into place only once they
	// are known to be complete. Writing straight to the destination means that
	// a link that drops, a full card, or the console losing power leaves a file
	// that has the right name and the wrong contents — and nothing afterwards
	// can tell that it is wrong.
	auto const partial = job.dest + ".part";

	File file;
	if (!file.open (partial, O_WRONLY | O_CREAT | O_TRUNC))
	{
		logger::printf (logger::ERROR, "Cannot write %s", partial.c_str ());
		return false;
	}

	net::Socket data;
	if (!client.beginRetrieve (job.source, data))
	{
		file.close ();
		std::remove (partial.c_str ());
		return false;
	}

	std::vector<char> buffer (chunkSize ());
	std::uint64_t done = 0;
	bool ok            = true;

	Timing timing;

	// recv() hands back about 4 KB at a time whatever size buffer it is given,
	// so writing each one straight to the card meant a full FS round trip per
	// 4 KB. Fill the chunk first and write it whole.
	std::size_t filled = 0;

	auto const flush = [&] () {
		if (filled == 0)
			return true;

		auto const before = platform::steady_clock::now ();
		auto const wrote  = file.write (buffer.data (), filled);
		timing.disk += platform::steady_clock::now () - before;
		++timing.diskCalls;

		filled = 0;

		if (!wrote)
		{
			// Almost always a full SD card, or a file past FAT32's 4 GB limit.
			logger::printf (logger::ERROR, "Write failed on %s", job.name.c_str ());
			return false;
		}

		return true;
	};

	auto const started = platform::steady_clock::now ();

	while (true)
	{
		if (cancelRequested (nullptr))
		{
			client.abortTransfer (data);
			ok = false;
			break;
		}

		auto const beforeRecv = platform::steady_clock::now ();
		auto const got        = data.recv (buffer.data () + filled,
            buffer.size () - filled,
            DATA_TIMEOUT_MS,
            cancelRequested,
            nullptr);
		timing.net += platform::steady_clock::now () - beforeRecv;
		++timing.netCalls;

		if (got == 0)
		{
			ok = flush ();
			break;
		}

		if (got < 0)
		{
			// -2 is our own timeout, -1 a socket error — or the cancellation
			// that interrupted the wait, which is not something to report as a
			// failure. Telling the first two apart is worth it: one means the
			// server went quiet, the other that the link broke.
			if (!cancelRequested (nullptr))
				logger::printf (logger::ERROR,
				    "%s after %llu bytes of %s",
				    got == -2 ? "Timed out" : "Socket error",
				    static_cast<unsigned long long> (done),
				    job.name.c_str ());

			// The server still owes a reply for this RETR. Leaving it unread is
			// what desynchronises the control connection, and from then on
			// every listing and every existence check answers the previous
			// question instead of the one being asked.
			client.abortTransfer (data);
			ok = false;
			break;
		}

		filled += static_cast<std::size_t> (got);
		done += got;
		reportProgress (job.id, done, got);

		if (filled == buffer.size () && !flush ())
		{
			client.abortTransfer (data);
			ok = false;
			break;
		}
	}

	file.close ();

	if (ok)
	{
		data.close ();
		ok = client.finishTransfer ();
	}

	// The size from the listing is the only independent check we have that the
	// bytes that arrived are the bytes the file is made of. A server that hangs
	// up mid-transfer can still answer 226, and without this the truncated
	// result would be accepted and renamed into place.
	if (ok && job.size > 0 && done != job.size)
	{
		logger::printf (logger::ERROR,
		    "%s: got %llu bytes, expected %llu",
		    job.name.c_str (),
		    static_cast<unsigned long long> (done),
		    static_cast<unsigned long long> (job.size));
		ok = false;
	}

	logTiming ("dl", job.name, done, timing, platform::steady_clock::now () - started);

	if (!ok)
	{
		std::remove (partial.c_str ()); // never leave a truncated file behind
		return false;
	}

	// Only now is the destination touched. rename() over an existing file is
	// not reliable on FAT through libctru, so clear the way first.
	std::remove (job.dest.c_str ());

	if (!fs::local::rename (partial, job.dest))
	{
		logger::printf (logger::ERROR, "Cannot move %s into place", job.name.c_str ());
		std::remove (partial.c_str ());
		return false;
	}

	return true;
}

bool runUpload (ftp::Client &client, transfer::Job const &job)
{
	File file;
	if (!file.open (job.source, O_RDONLY))
	{
		logger::printf (logger::ERROR, "Cannot read %s", job.source.c_str ());
		return false;
	}

	net::Socket data;
	if (!client.beginStore (job.dest, data))
	{
		file.close ();
		return false;
	}

	std::vector<char> buffer (chunkSize ());
	std::uint64_t done = 0;
	bool ok            = true;

	Timing timing;
	auto const started = platform::steady_clock::now ();

	while (true)
	{
		if (cancelRequested (nullptr))
		{
			client.abortTransfer (data);
			ok = false;
			break;
		}

		auto const beforeRead = platform::steady_clock::now ();
		auto const got        = file.read (buffer.data (), buffer.size ());
		timing.disk += platform::steady_clock::now () - beforeRead;
		++timing.diskCalls;

		if (got == 0)
			break;

		if (got < 0)
		{
			logger::printf (logger::ERROR, "Read failed on %s", job.source.c_str ());
			client.abortTransfer (data);
			ok = false;
			break;
		}

		auto const beforeSend = platform::steady_clock::now ();
		auto const sent = data.sendAll (buffer.data (), got, DATA_TIMEOUT_MS, cancelRequested, nullptr);
		timing.net += platform::steady_clock::now () - beforeSend;
		++timing.netCalls;

		if (!sent)
		{
			client.abortTransfer (data);
			ok = false;
			break;
		}

		done += got;
		reportProgress (job.id, done, got);
	}

	file.close ();

	if (ok)
	{
		// The server only completes the transfer once the data socket closes.
		data.close ();
		ok = client.finishTransfer ();
	}

	// Same check as the download side, from the other direction: ask the server
	// how big the file it just stored is. A server that only received half of
	// it will say so. Servers without SIZE simply fail the query, and that is
	// not treated as an error — it only means we cannot verify.
	if (ok && done > 0)
	{
		std::uint64_t remote = 0;
		if (client.size (job.dest, remote) && remote != done)
		{
			logger::printf (logger::ERROR,
			    "%s: server stored %llu bytes of %llu",
			    job.name.c_str (),
			    static_cast<unsigned long long> (remote),
			    static_cast<unsigned long long> (done));
			ok = false;
		}
	}

	logTiming ("ul", job.name, done, timing, platform::steady_clock::now () - started);

	// A partial file on the server is worse than no file: it looks complete to
	// anything that reads it later. Only worth attempting while the session
	// still works — on a dead one the command cannot be delivered anyway.
	if (!ok && client.alive ())
		client.removeFile (job.dest);

	return ok;
}

bool runLocalCopy (transfer::Job const &job)
{
	if (!fs::local::makeDirectories (fs::parent (job.dest)))
	{
		logger::printf (logger::ERROR, "Cannot create %s", fs::parent (job.dest).c_str ());
		return false;
	}

	File in;
	if (!in.open (job.source, O_RDONLY))
	{
		logger::printf (logger::ERROR, "Cannot read %s", job.source.c_str ());
		return false;
	}

	// Same scratch-file rule as a download: a copy that runs out of card
	// halfway must not leave something wearing the destination's name.
	auto const partial = job.dest + ".part";

	File out;
	if (!out.open (partial, O_WRONLY | O_CREAT | O_TRUNC))
	{
		logger::printf (logger::ERROR, "Cannot write %s", partial.c_str ());
		return false;
	}

	std::vector<char> buffer (chunkSize ());
	std::uint64_t done = 0;
	bool ok            = true;

	while (true)
	{
		if (cancelRequested (nullptr))
		{
			ok = false;
			break;
		}

		auto const got = in.read (buffer.data (), buffer.size ());
		if (got == 0)
			break;

		if (got < 0 || !out.write (buffer.data (), got))
		{
			logger::printf (logger::ERROR, "Copy of %s failed", job.name.c_str ());
			ok = false;
			break;
		}

		done += got;
		reportProgress (job.id, done, got);
	}

	in.close ();
	out.close ();

	if (!ok)
	{
		std::remove (partial.c_str ());
		return false;
	}

	std::remove (job.dest.c_str ());

	if (!fs::local::rename (partial, job.dest))
	{
		logger::printf (logger::ERROR, "Cannot move %s into place", job.name.c_str ());
		std::remove (partial.c_str ());
		return false;
	}

	return true;
}

/// \brief The worker task: expand every queued request, then run every job.
void drain (ftp::Client &client)
{
	while (true)
	{
		// Jobs before requests: a request is only expanded once everything
		// queued ahead of it has run, so a request that depends on an earlier
		// one (uploading what was just downloaded) sees the finished result.
		transfer::Job job;
		bool haveJob = false;

		{
			Guard guard;

			// Paused: stop draining entirely. resume() posts drain again, so
			// nothing is lost by unwinding here rather than spinning.
			if (s_paused)
			{
				s_draining = false;
				s_activeId = 0;
				return;
			}

			for (auto &candidate : s_jobs)
			{
				if (candidate.status != transfer::PENDING)
					continue;

				candidate.status = transfer::ACTIVE;
				job              = candidate;
				s_activeId       = candidate.id;
				s_cancelActive   = false;
				haveJob          = true;
				break;
			}
		}

		if (!haveJob)
		{
			transfer::Request request;
			bool haveRequest = false;

			{
				Guard guard;
				if (!s_requests.empty ())
				{
					request     = std::move (s_requests.front ());
					s_requests.pop_front ();
					haveRequest = true;
					s_expanding = true;
				}
				else
				{
					s_draining = false;
					s_activeId = 0;
					return;
				}
			}

			if (haveRequest)
			{
				std::vector<transfer::Job> jobs;
				// A local copy needs no server; anything else against a dead
				// session is not worth walking at all.
				auto const usable = request.direction == transfer::LOCAL_COPY || client.alive ();

				auto const walked =
				    usable && expand (client,
				                  request.direction,
				                  request.source,
				                  request.destDir,
				                  request.overwrite,
				                  0,
				                  jobs);

				Guard guard;
				s_expanding = false;

				if (!walked)
				{
					// Nothing is queued from a walk that did not finish. The
					// jobs it did produce describe a tree we never managed to
					// read, so running them would build something that does not
					// match the source — and silently.
					logger::printf (logger::ERROR,
					    "Skipped %s: could not read it in full",
					    request.source.c_str ());
					continue;
				}

				for (auto &newJob : jobs)
				{
					newJob.id = s_nextId++;
					s_jobs.push_back (std::move (newJob));
				}
			}

			continue;
		}

		resetRate ();

		// Fail fast on a session that is already gone. Each of these would
		// otherwise sit through its own connect and command timeouts, so a
		// queue of two hundred files after a dropped link means minutes of an
		// app that looks hung and reports nothing until the very end.
		if (job.direction != transfer::LOCAL_COPY && !client.alive ())
		{
			Guard guard;
			if (auto *const stored = findJob (job.id))
				stored->status = transfer::FAILED;

			s_activeId     = 0;
			s_cancelActive = false;
			continue;
		}

		bool ok;
		if (job.directory)
			ok = runDirectory (client, job);
		else if (job.direction == transfer::DOWNLOAD)
			ok = runDownload (client, job);
		else if (job.direction == transfer::UPLOAD)
			ok = runUpload (client, job);
		else
			ok = runLocalCopy (job);

		Guard guard;
		if (auto *const stored = findJob (job.id))
		{
			if (ok)
			{
				stored->status = transfer::DONE;
				stored->done   = stored->size;
			}
			else
				stored->status = s_cancelActive ? transfer::CANCELLED : transfer::FAILED;
		}

		s_activeId     = 0;
		s_cancelActive = false;
	}
}
}

void transfer::init ()
{
	LightLock_Init (&s_lock);
}

void transfer::enqueue (Request request)
{
	bool start = false;

	{
		Guard guard;
		s_requests.push_back (std::move (request));

		if (!s_draining)
		{
			s_draining = true;
			start      = true;
		}
	}

	if (start)
		session::post (drain);
}

void transfer::pause ()
{
	Guard guard;
	s_paused = true;
}

void transfer::resume ()
{
	bool start = false;

	{
		Guard guard;
		s_paused = false;

		auto const work = !s_requests.empty () ||
		                  std::any_of (s_jobs.begin (), s_jobs.end (), [] (Job const &job) {
			                  return job.status == PENDING;
		                  });

		if (work && !s_draining)
		{
			s_draining = true;
			start      = true;
		}
	}

	if (start)
		session::post (drain);
}

void transfer::snapshot (std::vector<Job> &out)
{
	Guard guard;
	out = s_jobs;
}

bool transfer::active ()
{
	Guard guard;

	if (s_expanding || !s_requests.empty ())
		return true;

	return std::any_of (s_jobs.begin (), s_jobs.end (), [] (Job const &job) {
		return job.status == PENDING || job.status == ACTIVE;
	});
}

bool transfer::expanding ()
{
	Guard guard;
	return s_expanding;
}

int transfer::pending (bool &atLeast)
{
	Guard guard;

	// A request being walked right now has already left the deque, so its files
	// are not counted anywhere yet either.
	atLeast = !s_requests.empty () || s_expanding;

	int count = 0;
	for (auto const &job : s_jobs)
	{
		// Directory jobs only create the folder; counting them would overstate
		// how much is left to move.
		if (job.directory)
			continue;

		if (job.status == PENDING || job.status == ACTIVE)
			++count;
	}

	// Every request that has not been expanded yet is worth at least one file.
	count += static_cast<int> (s_requests.size ());

	return count;
}

void transfer::cancel (unsigned const id)
{
	Guard guard;

	auto *const job = findJob (id);
	if (!job)
		return;

	if (job->status == ACTIVE)
		s_cancelActive = true;
	else if (job->status == PENDING)
		job->status = CANCELLED;
}

void transfer::cancelPending ()
{
	Guard guard;

	s_requests.clear ();

	for (auto &job : s_jobs)
	{
		if (job.status == PENDING)
			job.status = CANCELLED;
	}
}

void transfer::cancelAll ()
{
	Guard guard;

	s_requests.clear ();
	s_cancelActive = true;

	for (auto &job : s_jobs)
	{
		if (job.status == PENDING)
			job.status = CANCELLED;
	}
}

double transfer::rate ()
{
	Guard guard;
	return s_activeId ? s_rate : 0.0;
}

int transfer::eta ()
{
	Guard guard;

	if (!s_activeId || s_rate <= 1.0)
		return -1;

	auto const *const job = findJob (s_activeId);
	if (!job || job->size <= job->done)
		return -1;

	return static_cast<int> ((job->size - job->done) / s_rate);
}

void transfer::clearFinished ()
{
	Guard guard;

	s_jobs.erase (std::remove_if (s_jobs.begin (),
	                  s_jobs.end (),
	                  [] (Job const &job) {
		                  return job.status == DONE || job.status == CANCELLED ||
		                         job.status == FAILED;
	                  }),
	    s_jobs.end ());
}
