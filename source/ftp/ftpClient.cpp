#include "ftpClient.h"

#include "ftpListing.h"
#include "log.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
/// Control replies are short; data reads use the transfer buffer instead.
constexpr int CONTROL_TIMEOUT_MS = 10000;
constexpr int DATA_TIMEOUT_MS    = 30000;

/// Closing down is best-effort: if the server or the link is already gone,
/// waiting the full control timeout for a QUIT nobody will answer just makes
/// disconnecting look like a hang.
constexpr int CLOSING_TIMEOUT_MS = 2000;

/// A single control line cannot legitimately reach this. Without a cap, a peer
/// that sends bytes but never a newline grows the buffer until the heap gives
/// out, and on this console that is a crash rather than an exception.
constexpr std::size_t MAX_LINE = 8 * 1024;

/// Neither can one directory listing. A server that streams without end would
/// otherwise take the app down with it.
constexpr std::size_t MAX_LISTING = 4 * 1024 * 1024;

bool isRootPath (std::string const &path)
{
	return path.empty () || path == "/";
}
}

ftp::Client::~Client ()
{
	disconnect ();
}

void ftp::Client::fail (char const *const reason)
{
	if (m_broken)
		return;

	m_broken = true;

	if (m_control)
	{
		logger::printf (logger::ERROR, "Control connection lost (%s)", reason);
		m_control.close ();
	}

	m_buffer.clear ();
}

bool ftp::Client::sendCommand (std::string const &command)
{
	if (!alive ())
		return false;

	// Passwords must never reach the console window.
	if (command.compare (0, 5, "PASS ") == 0)
		logger::add (logger::SENT, "PASS ********");
	else
		logger::add (logger::SENT, command);

	auto const line = command + "\r\n";
	if (m_control.sendAll (line.data (), line.size (), CONTROL_TIMEOUT_MS))
		return true;

	fail ("send");
	return false;
}

bool ftp::Client::readLine (std::string &line)
{
	while (true)
	{
		auto const eol = m_buffer.find ('\n');
		if (eol != std::string::npos)
		{
			line = m_buffer.substr (0, eol);
			m_buffer.erase (0, eol + 1);

			if (!line.empty () && line.back () == '\r')
				line.pop_back ();

			return true;
		}

		if (m_buffer.size () > MAX_LINE)
		{
			fail ("oversized reply");
			return false;
		}

		if (!alive ())
			return false;

		char chunk[512];
		auto const got = m_control.recv (chunk, sizeof (chunk), CONTROL_TIMEOUT_MS);
		if (got <= 0)
		{
			// 0 is the server closing on us, -2 our own timeout, -1 a socket
			// error. All three mean the same thing here: no more replies.
			fail (got == 0 ? "closed by server" : got == -2 ? "timeout" : "socket error");
			return false;
		}

		m_buffer.append (chunk, got);
	}
}

bool ftp::Client::readResponse (int &code, std::string &text)
{
	std::string line;
	if (!readLine (line))
		return false;

	logger::add (logger::RECEIVED, line);

	// Anything that is not "NNN " or "NNN-" means we are no longer reading
	// replies at the boundaries the server is writing them, and every later
	// command would get the previous one's answer.
	if (line.size () < 4 || !std::isdigit (static_cast<unsigned char> (line[0])) ||
	    !std::isdigit (static_cast<unsigned char> (line[1])) ||
	    !std::isdigit (static_cast<unsigned char> (line[2])) ||
	    (line[3] != ' ' && line[3] != '-'))
	{
		fail ("malformed reply");
		return false;
	}

	code = std::atoi (line.substr (0, 3).c_str ());
	text = line.substr (4);

	// A '-' after the code opens a multi-line reply that ends with the same
	// code followed by a space.
	if (line[3] != '-')
		return true;

	auto const terminator = line.substr (0, 3) + " ";

	// FEAT on a chatty server runs to a few dozen lines; a thousand means the
	// terminator is never coming and we would read until the timeout, forever.
	for (int i = 0; i < 1000 && readLine (line); ++i)
	{
		logger::add (logger::RECEIVED, line);

		if (line.compare (0, terminator.size (), terminator) == 0)
		{
			text += "\n" + line.substr (4);
			return true;
		}

		text += "\n" + line;
	}

	if (alive ())
		fail ("unterminated multi-line reply");

	return false;
}

bool ftp::Client::command (std::string const &cmd, int &code, std::string &text)
{
	return sendCommand (cmd) && readResponse (code, text);
}

bool ftp::Client::simpleCommand (std::string const &cmd)
{
	int code = 0;
	std::string text;

	if (!command (cmd, code, text))
		return false;

	return code >= 200 && code < 300;
}

bool ftp::Client::connect (std::string const &host,
    std::uint16_t const port,
    std::string const &user,
    std::string const &password)
{
	disconnect ();

	m_host   = host;
	m_port   = port;
	m_broken = false;

	// The control connection only ever carries short command lines, so it keeps
	// the stack's default buffers.
	if (!m_control.connect (host, port))
		return false;

	int code = 0;
	std::string text;

	// greeting
	if (!readResponse (code, text) || code != 220)
	{
		disconnect ();
		return false;
	}

	if (!command ("USER " + user, code, text))
	{
		disconnect ();
		return false;
	}

	// 331 asks for a password; 230 means the server logged us in without one.
	if (code == 331)
	{
		if (!command ("PASS " + password, code, text) || code != 230)
		{
			logger::add (logger::ERROR, "Login rejected");
			disconnect ();
			return false;
		}
	}
	else if (code != 230)
	{
		logger::add (logger::ERROR, "Login rejected");
		disconnect ();
		return false;
	}

	// UTF-8 is opportunistic: servers that do not advertise it are assumed to
	// pass bytes through unchanged, which is what modern ones do anyway.
	if (command ("FEAT", code, text) && code == 211)
	{
		std::string upper = text;
		std::transform (upper.begin (), upper.end (), upper.begin (), ::toupper);
		m_utf8 = upper.find ("UTF8") != std::string::npos;
	}

	if (m_utf8)
		simpleCommand ("OPTS UTF8 ON");

	// Everything is transferred in binary, per the spec.
	if (!simpleCommand ("TYPE I"))
	{
		disconnect ();
		return false;
	}

	logger::printf (logger::INFO, "Connected to %s:%u", host.c_str (), port);
	return true;
}

void ftp::Client::disconnect ()
{
	if (m_control)
	{
		// Only worth attempting on a session that still works, and even then
		// without waiting around: the socket is closed either way.
		if (!m_broken)
		{
			auto const line = std::string ("QUIT\r\n");
			logger::add (logger::SENT, "QUIT");
			m_control.sendAll (line.data (), line.size (), CLOSING_TIMEOUT_MS);
		}

		m_control.close ();
	}

	m_buffer.clear ();
	m_utf8   = false;
	m_broken = false;
}

bool ftp::Client::openPassive (net::Socket &data)
{
	int code = 0;
	std::string text;

	if (!command ("PASV", code, text) || code != 227)
		return false;

	// "227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)"
	auto const open = text.find ('(');
	if (open == std::string::npos)
		return false;

	unsigned h[4], p[2];
	if (std::sscanf (text.c_str () + open + 1,
	        "%u,%u,%u,%u,%u,%u",
	        &h[0],
	        &h[1],
	        &h[2],
	        &h[3],
	        &p[0],
	        &p[1]) != 6)
		return false;

	// Every field is one byte. Out-of-range values mean we misread the reply,
	// and connecting to whatever they truncate to is a ten-second wait for a
	// timeout at best.
	for (auto const value : {h[0], h[1], h[2], h[3], p[0], p[1]})
	{
		if (value > 255)
		{
			logger::add (logger::ERROR, "Malformed PASV reply");
			return false;
		}
	}

	auto const port = static_cast<std::uint16_t> ((p[0] << 8) | p[1]);
	if (port == 0)
	{
		logger::add (logger::ERROR, "Malformed PASV reply");
		return false;
	}

	char address[16];
	std::snprintf (address, sizeof (address), "%u.%u.%u.%u", h[0], h[1], h[2], h[3]);

	// Servers behind NAT, and a few that are simply misconfigured, advertise an
	// address they are not reachable at — the classic case being a LAN server
	// announcing its WAN address, or 0.0.0.0. Connecting there is a guaranteed
	// timeout. The control connection is by definition reachable, so when the
	// two disagree, trust the one already working. Every serious FTP client
	// does this; the data connection is meant to come from the same host.
	auto const peer = m_control.peerAddress ();
	if (!peer.empty () && peer != address)
	{
		logger::printf (logger::INFO,
		    "PASV says %s; using the control address %s instead",
		    address,
		    peer.c_str ());

		return data.connect (peer, port);
	}


	// Both buffers are left at the stack's own defaults, on measurement.
	//
	// A 64 KB SO_SNDBUF looked like a win at first — uploads jumped to about
	// 500 KB/s — but it did not hold up: they slowed right down again, while a
	// 64 KB SO_RCVBUF dropped downloads to ~10 KB/s with stalls and failures
	// part way through. The wifi module has to back these buffers with its own
	// memory, and asking it for more than it can hold costs more than it gains.
	//
	// connect() still takes the sizes, so this is one edit to revisit with a
	// smaller figure if it ever proves worth it.
	return data.connect (address, port);
}

bool ftp::Client::openData (std::string const &cmd, net::Socket &data)
{
	if (!openPassive (data))
		return false;

	int code = 0;
	std::string text;

	if (!command (cmd, code, text))
	{
		data.close ();
		return false;
	}

	// 1xx means the data connection is now live.
	if (code < 100 || code >= 200)
	{
		data.close ();
		return false;
	}

	return true;
}

bool ftp::Client::list (std::string const &path, fs::Listing &out)
{
	out.clear ();

	auto const target = isRootPath (path) ? "/" : path;

	// MLSD first: it reports type and size unambiguously. Servers without it
	// answer 5xx and we retry with LIST.
	bool machineReadable = true;
	net::Socket data;

	if (!openData ("MLSD " + target, data))
	{
		machineReadable = false;
		if (!openData ("LIST " + target, data))
			return false;
	}

	std::string buffer;
	char chunk[4096];

	while (true)
	{
		auto const got = data.recv (chunk, sizeof (chunk), DATA_TIMEOUT_MS);
		if (got < 0)
		{
			logger::printf (logger::ERROR,
			    "%s listing %s",
			    got == -2 ? "Timed out" : "Socket error",
			    target.c_str ());

			// Resynchronise rather than just closing: the server still owes a
			// reply for this transfer, and leaving it unread would make the
			// *next* command read it as its own answer.
			abortTransfer (data);
			return false;
		}

		if (got == 0)
			break;

		if (buffer.size () + got > MAX_LISTING)
		{
			logger::printf (logger::ERROR, "Listing of %s is too large", target.c_str ());
			abortTransfer (data);
			return false;
		}

		buffer.append (chunk, got);
	}

	data.close ();

	if (!finishTransfer ())
		return false;

	std::size_t pos = 0;
	while (pos < buffer.size ())
	{
		auto eol = buffer.find ('\n', pos);
		if (eol == std::string::npos)
			eol = buffer.size ();

		auto line = buffer.substr (pos, eol - pos);
		pos       = eol + 1;

		if (!line.empty () && line.back () == '\r')
			line.pop_back ();

		if (line.empty ())
			continue;

		fs::Entry entry;
		if (machineReadable ? parseMlsdLine (line, entry) : parseListLine (line, entry))
			out.push_back (std::move (entry));
	}

	std::sort (out.begin (), out.end (), [] (fs::Entry const &a, fs::Entry const &b) {
		if (a.isDir () != b.isDir ())
			return a.isDir ();

		return ::strcasecmp (a.name.c_str (), b.name.c_str ()) < 0;
	});

	return true;
}

bool ftp::Client::makeDirectory (std::string const &path)
{
	int code = 0;
	std::string text;

	// 257 is the documented success code, but some servers answer 250.
	return command ("MKD " + path, code, text) && (code == 257 || code == 250);
}

bool ftp::Client::removeFile (std::string const &path)
{
	return simpleCommand ("DELE " + path);
}

bool ftp::Client::removeDirectory (std::string const &path)
{
	return simpleCommand ("RMD " + path);
}

bool ftp::Client::rename (std::string const &from, std::string const &to)
{
	int code = 0;
	std::string text;

	if (!command ("RNFR " + from, code, text) || code != 350)
		return false;

	return simpleCommand ("RNTO " + to);
}

bool ftp::Client::size (std::string const &path, std::uint64_t &out)
{
	int code = 0;
	std::string text;

	if (!command ("SIZE " + path, code, text) || code != 213)
		return false;

	out = std::strtoull (text.c_str (), nullptr, 10);
	return true;
}

ftp::Client::Probe ftp::Client::probe (std::string const &path, std::uint64_t &size)
{
	size = 0;

	int code = 0;
	std::string text;

	if (!command ("SIZE " + path, code, text))
		return PROBE_ERROR; // no reply at all: the session is gone

	if (code == 213)
	{
		size = std::strtoull (text.c_str (), nullptr, 10);
		return PROBE_FILE;
	}

	// The server answered and declined. 550 is what every server uses for "not
	// a plain file", which for our purposes means a directory — but only a
	// listing can actually confirm it, and the caller does exactly that next.
	if (code >= 500 && code < 600)
		return PROBE_DIRECTORY;

	logger::printf (logger::ERROR, "SIZE %s: unexpected reply %d", path.c_str (), code);
	return PROBE_ERROR;
}

bool ftp::Client::exists (std::string const &path)
{
	std::uint64_t bytes;
	if (size (path, bytes))
		return true;

	// SIZE fails on directories on most servers, so fall back to asking the
	// parent for a listing and looking for the name.
	fs::Listing listing;
	if (!list (fs::parent (path), listing))
		return false;

	auto const name = fs::basename (path);
	return std::any_of (listing.begin (), listing.end (), [&name] (fs::Entry const &entry) {
		return entry.name == name;
	});
}

bool ftp::Client::beginRetrieve (std::string const &path, net::Socket &data)
{
	return openData ("RETR " + path, data);
}

bool ftp::Client::beginStore (std::string const &path, net::Socket &data)
{
	return openData ("STOR " + path, data);
}

bool ftp::Client::finishTransfer ()
{
	int code = 0;
	std::string text;

	if (!readResponse (code, text))
		return false;

	return code >= 200 && code < 300;
}

void ftp::Client::abortTransfer (net::Socket &data)
{
	// Dropping the data socket makes the server finish the transfer with a
	// 426; ABOR then gets its own reply. Both have to be consumed or the
	// control connection stays out of sync for every later command.
	data.close ();

	if (!alive ())
		return;

	if (!sendCommand ("ABOR"))
		return;

	int code = 0;
	std::string text;

	for (int i = 0; i < 2; ++i)
	{
		if (!readResponse (code, text))
		{
			// Resyncing is the whole point of this function, so failing at it
			// means the session cannot be trusted for anything afterwards.
			// fail() has already latched that; nothing more to do here.
			return;
		}

		// 226/426 close the transfer; 2xx after that is ABOR's own reply.
		if (code >= 200 && code < 300 && i > 0)
			break;
	}
}
