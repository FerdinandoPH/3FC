#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace net
{
/// \brief RAII socket.
///
/// Every blocking operation goes through poll() with a short timeout so the
/// transfer worker can notice a cancellation request promptly instead of
/// sitting in a recv() until the TCP stack gives up.
class Socket
{
public:
	~Socket ();
	Socket () = default;

	Socket (Socket const &) = delete;
	Socket &operator= (Socket const &) = delete;

	Socket (Socket &&that) noexcept;
	Socket &operator= (Socket &&that) noexcept;

	explicit operator bool () const
	{
		return m_fd >= 0;
	}

	int fd () const
	{
		return m_fd;
	}

	/// \brief Connect to host:port.
	///
	/// Send and receive buffers are separate because on this console they are
	/// not symmetric: a large send buffer measurably speeds uploads up, while a
	/// large *receive* buffer made downloads collapse and start erroring out —
	/// the wifi module has to actually back that memory, and asking for more
	/// than it can hold appears to cost far more than it gains.
	///
	/// \param timeoutMs Total time to wait before giving up
	/// \param sndBuf SO_SNDBUF to ask for, 0 to leave the default
	/// \param rcvBuf SO_RCVBUF to ask for, 0 to leave the default
	///
	/// Both are applied before connect(): the receive window is advertised
	/// during the handshake, so setting it afterwards is too late.
	bool connect (std::string const &host,
	    std::uint16_t port,
	    int timeoutMs = 10000,
	    int sndBuf    = 0,
	    int rcvBuf    = 0);

	/// \brief Adopt an already-connected descriptor.
	void adopt (int fd);

	/// \brief Dotted-quad address of the peer, empty if not connected.
	std::string peerAddress () const;

	void close ();

	/// \brief Wait until the socket is readable.
	/// \returns true if readable, false on timeout or error
	bool waitReadable (int timeoutMs) const;

	/// \brief Read whatever is available.
	///
	/// \param cancel Polled between waits; reading gives up early if it returns
	///        true. Without it a server that goes quiet holds the worker for the
	///        full timeout, which is how cancelling a transfer — or quitting the
	///        app — used to take half a minute to be noticed.
	/// \returns bytes read, 0 on clean EOF, -1 on error or cancellation, -2 on
	///          timeout
	std::ptrdiff_t recv (void *buffer,
	    std::size_t size,
	    int timeoutMs,
	    bool (*cancel) (void *) = nullptr,
	    void *cancelArg         = nullptr);

	/// \brief Write the whole buffer.
	/// \param cancel Polled between chunks; sending stops early if it returns true
	/// \returns true if everything was written
	bool sendAll (void const *buffer,
	    std::size_t size,
	    int timeoutMs,
	    bool (*cancel) (void *) = nullptr,
	    void *cancelArg         = nullptr);

private:
	int m_fd = -1;
};
}
