#include "socket.h"

#include "log.h"
#include "platform.h"

#include <3ds.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace
{
/// Longest a single poll() waits, so cancellation is noticed promptly.
constexpr int POLL_SLICE_MS = 250;

bool setNonBlocking (int const fd, bool const nonBlocking)
{
	auto flags = ::fcntl (fd, F_GETFL, 0);
	if (flags < 0)
		return false;

	flags = nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
	return ::fcntl (fd, F_SETFL, flags) == 0;
}
}

net::Socket::~Socket ()
{
	close ();
}

net::Socket::Socket (Socket &&that) noexcept : m_fd (that.m_fd)
{
	that.m_fd = -1;
}

net::Socket &net::Socket::operator= (Socket &&that) noexcept
{
	if (this != &that)
	{
		close ();
		m_fd      = that.m_fd;
		that.m_fd = -1;
	}

	return *this;
}

void net::Socket::adopt (int const fd)
{
	close ();
	m_fd = fd;
}

std::string net::Socket::peerAddress () const
{
	if (m_fd < 0)
		return {};

	sockaddr_in peer{};
	socklen_t len = sizeof (peer);
	if (::getpeername (m_fd, reinterpret_cast<sockaddr *> (&peer), &len) != 0)
		return {};

	char text[INET_ADDRSTRLEN]{};
	if (!::inet_ntop (AF_INET, &peer.sin_addr, text, sizeof (text)))
		return {};

	return text;
}

void net::Socket::close ()
{
	if (m_fd >= 0)
	{
		::closesocket (m_fd);
		m_fd = -1;
	}
}

bool net::Socket::connect (std::string const &host,
    std::uint16_t const port,
    int const timeoutMs,
    int const sndBuf,
    int const rcvBuf)
{
	close ();

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port   = htons (port);

	if (::inet_pton (AF_INET, host.c_str (), &addr.sin_addr) != 1)
	{
		// Not a literal address, so resolve it. libctru's resolver is
		// blocking, but this only happens once per connection attempt.
		auto *const entry = ::gethostbyname (host.c_str ());
		if (!entry || entry->h_addrtype != AF_INET || !entry->h_addr_list[0])
		{
			logger::printf (logger::ERROR, "Could not resolve %s", host.c_str ());
			return false;
		}

		std::memcpy (&addr.sin_addr, entry->h_addr_list[0], sizeof (addr.sin_addr));
	}

	auto const fd = ::socket (AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
		logger::printf (logger::ERROR, "socket: %s", std::strerror (errno));
		return false;
	}

	if (sndBuf > 0)
		::setsockopt (fd, SOL_SOCKET, SO_SNDBUF, &sndBuf, sizeof (sndBuf));
	if (rcvBuf > 0)
		::setsockopt (fd, SOL_SOCKET, SO_RCVBUF, &rcvBuf, sizeof (rcvBuf));

	if (sndBuf > 0 || rcvBuf > 0)
	{
		// Log what we actually got: a request the stack silently ignores or
		// silently clamps is invisible otherwise, and the difference decides
		// whether this knob is worth turning at all.
		int snd = 0, rcv = 0;
		socklen_t sndLen = sizeof (snd), rcvLen = sizeof (rcv);

		auto const sndOk = ::getsockopt (fd, SOL_SOCKET, SO_SNDBUF, &snd, &sndLen) == 0;
		auto const rcvOk = ::getsockopt (fd, SOL_SOCKET, SO_RCVBUF, &rcv, &rcvLen) == 0;

		logger::printf (logger::DEBUG,
		    "bufs snd %d->%s%d  rcv %d->%s%d",
		    sndBuf,
		    sndOk ? "" : "?",
		    snd,
		    rcvBuf,
		    rcvOk ? "" : "?",
		    rcv);
	}

	setNonBlocking (fd, true);

	logger::printf (logger::DEBUG,
	    "connect %s:%u fd=%d",
	    host.c_str (),
	    static_cast<unsigned> (port),
	    fd);

	// libctru's SOC reports a non-blocking connect in progress as EAGAIN rather
	// than the POSIX EINPROGRESS, so both have to be treated as "pending".
	if (::connect (fd, reinterpret_cast<sockaddr *> (&addr), sizeof (addr)) < 0)
	{
		auto const err = errno;
		if (err != EINPROGRESS && err != EAGAIN && err != EWOULDBLOCK && err != EALREADY)
		{
			logger::printf (logger::ERROR, "connect: %s (%d)", std::strerror (err), err);
			::closesocket (fd);
			return false;
		}

		logger::printf (logger::DEBUG, "connect pending: %s (%d)", std::strerror (err), err);
	}

	// Wall-clock deadline rather than a countdown of poll slices: on hardware
	// poll() can return immediately and repeatedly while the connect is still
	// in flight, which would otherwise burn the whole budget in microseconds.
	auto const start = platform::steady_clock::now ();
	auto const limit = std::chrono::milliseconds (timeoutMs);

	// The SO_ERROR reading below is only interesting once; the loop spins every
	// 10 ms and would otherwise flood the console ring buffer.
	bool reported = false;

	while (platform::steady_clock::now () - start < limit)
	{
		pollfd pfd{fd, POLLOUT, 0};

		auto const rc = ::poll (&pfd, 1, POLL_SLICE_MS);
		if (rc < 0)
		{
			if (errno == EINTR)
				continue;

			logger::printf (logger::ERROR, "poll: %s (%d)", std::strerror (errno), errno);
			break;
		}

		if (rc == 0)
			continue;

		if (pfd.revents & (POLLERR | POLLNVAL))
		{
			logger::printf (logger::ERROR,
			    "connect to %s:%u failed (revents 0x%02x)",
			    host.c_str (),
			    static_cast<unsigned> (port),
			    static_cast<unsigned> (pfd.revents));
			::closesocket (fd);
			return false;
		}

		if (!(pfd.revents & POLLOUT))
		{
			// Not writable yet. 3DS SOCU also likes to raise POLLHUP on a socket
			// that is still negotiating, so this is not by itself a failure.
			svcSleepThread (10'000'000ULL);
			continue;
		}

		// Deliberately not consulted for the verdict: the 3DS hands back the
		// SO_ERROR option value verbatim, so it is a native SOCU code and not a
		// newlib errno (a connect still in flight shows up as -26, which is
		// index 0x1A of libctru's alphabetical error table, EINPROGRESS).
		// Comparing it against POSIX constants is meaningless; it is only
		// logged because it is useful when something does go wrong.
		int error     = 0;
		socklen_t len = sizeof (error);
		if (!reported && ::getsockopt (fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error != 0)
		{
			logger::printf (logger::DEBUG, "SO_ERROR %d (native)", error);
			reported = true;
		}

		// getpeername is the authority: it only succeeds once the handshake has
		// completed, and unlike SO_ERROR it means the same thing everywhere. Any
		// failure means "not connected yet"; a connection that never establishes
		// is caught by the deadline below.
		sockaddr_in peer{};
		socklen_t peerLen = sizeof (peer);
		if (::getpeername (fd, reinterpret_cast<sockaddr *> (&peer), &peerLen) != 0)
		{
			svcSleepThread (10'000'000ULL);
			continue;
		}

		// Left non-blocking on purpose: recv/sendAll below try the socket first
		// and only fall back to poll() when it would block, which halves the
		// number of IPC round trips to the SOC service during a transfer.
		m_fd = fd;
		return true;
	}

	logger::printf (logger::ERROR, "connect to %s:%u timed out", host.c_str (), port);
	::closesocket (fd);
	return false;
}

bool net::Socket::waitReadable (int const timeoutMs) const
{
	if (m_fd < 0)
		return false;

	pollfd pfd{m_fd, POLLIN, 0};
	return ::poll (&pfd, 1, timeoutMs) > 0 && (pfd.revents & (POLLIN | POLLHUP));
}

std::ptrdiff_t net::Socket::recv (void *const buffer,
    std::size_t const size,
    int const timeoutMs,
    bool (*const cancel) (void *),
    void *const cancelArg)
{
	if (m_fd < 0)
		return -1;

	auto remaining = timeoutMs;

	while (true)
	{
		if (cancel && cancel (cancelArg))
			return -1;

		// Read first, poll only if that would block. During a transfer the data
		// is almost always already there, and skipping the poll halves the IPC
		// round trips — which on this console is what the time actually goes on.
		auto const got = ::recv (m_fd, buffer, size, 0);
		if (got >= 0)
			return got;

		if (errno == EINTR)
			continue;

		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			logger::printf (logger::ERROR, "recv: %s", std::strerror (errno));
			return -1;
		}

		if (remaining <= 0)
			return -2;

		auto const slice = remaining < POLL_SLICE_MS ? remaining : POLL_SLICE_MS;
		pollfd pfd{m_fd, POLLIN, 0};

		auto const rc = ::poll (&pfd, 1, slice);
		if (rc < 0 && errno != EINTR)
			return -1;

		remaining -= slice;
	}
}

bool net::Socket::sendAll (void const *const buffer,
    std::size_t const size,
    int const timeoutMs,
    bool (*const cancel) (void *),
    void *const cancelArg)
{
	if (m_fd < 0)
		return false;

	auto const *data = static_cast<std::uint8_t const *> (buffer);
	std::size_t sent = 0;

	auto remaining = timeoutMs;

	while (sent < size)
	{
		if (cancel && cancel (cancelArg))
			return false;

		// Same as recv: try the socket first, poll only when it pushes back.
		auto const wrote = ::send (m_fd, data + sent, size - sent, 0);
		if (wrote > 0)
		{
			sent += wrote;
			remaining = timeoutMs; // progress, so the clock starts over
			continue;
		}

		if (wrote == 0)
			return false;

		if (errno == EINTR)
			continue;

		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			logger::printf (logger::ERROR, "send: %s", std::strerror (errno));
			return false;
		}

		if (remaining <= 0)
			return false;

		auto const slice = remaining < POLL_SLICE_MS ? remaining : POLL_SLICE_MS;
		pollfd pfd{m_fd, POLLOUT, 0};

		auto const rc = ::poll (&pfd, 1, slice);
		if (rc < 0 && errno != EINTR)
			return false;

		remaining -= slice;
	}

	return true;
}
