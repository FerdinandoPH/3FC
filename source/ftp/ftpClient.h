#pragma once

#include "fs/entry.h"
#include "net/socket.h"

#include <cstdint>
#include <string>

namespace ftp
{
/// \brief A single FTP control connection, always passive and always binary.
///
/// Not thread-safe: only the transfer worker touches it once connected. The UI
/// thread reads cached listings instead.
class Client
{
public:
	~Client ();
	Client () = default;

	bool connected () const
	{
		return static_cast<bool> (m_control);
	}

	/// \brief Whether the control connection is still usable.
	///
	/// Different from connected(): a session whose control stream broke or lost
	/// sync is closed and latched as unusable, so every later command fails at
	/// once instead of waiting out its own timeout. That distinction is what
	/// keeps a dropped wifi link from turning into minutes of apparent freeze.
	bool alive () const
	{
		return m_control && !m_broken;
	}

	/// What a path on the server turned out to be.
	enum Probe
	{
		PROBE_FILE,
		PROBE_DIRECTORY,
		PROBE_ERROR, ///< could not be determined — never guess from this
	};

	/// \brief Decide whether a remote path is a file or a directory.
	///
	/// SIZE succeeding means "file"; SIZE failing means "directory" *only* if
	/// the server actually answered. Treating an I/O failure as a directory is
	/// how a dead link ends up creating folders named after files.
	Probe probe (std::string const &path, std::uint64_t &size);

	/// \brief Log in and negotiate UTF-8 and binary mode.
	bool connect (std::string const &host,
	    std::uint16_t port,
	    std::string const &user,
	    std::string const &password);

	void disconnect ();

	/// \brief Directory listing, sorted directories-first then by name.
	/// Contains real entries only; the ".." row is the browser's business.
	bool list (std::string const &path, fs::Listing &out);

	bool makeDirectory (std::string const &path);
	bool removeFile (std::string const &path);
	bool removeDirectory (std::string const &path);
	bool rename (std::string const &from, std::string const &to);

	/// \brief Size of a remote file, used to detect overwrites.
	bool size (std::string const &path, std::uint64_t &out);

	/// \brief Whether a remote path exists at all.
	bool exists (std::string const &path);

	/// \brief Start a download. On success \a data is the open data connection;
	/// read it to EOF then call finishTransfer().
	bool beginRetrieve (std::string const &path, net::Socket &data);

	/// \brief Start an upload. Write to \a data, close it, then call
	/// finishTransfer().
	bool beginStore (std::string const &path, net::Socket &data);

	/// \brief Read the server's closing response after a data transfer.
	bool finishTransfer ();

	/// \brief Abort an in-flight transfer: the data socket is dropped and the
	/// control connection resynchronised so the session stays usable.
	void abortTransfer (net::Socket &data);

	std::string const &host () const
	{
		return m_host;
	}

	std::uint16_t port () const
	{
		return m_port;
	}

private:
	/// \brief Tear the control connection down and latch it as unusable.
	///
	/// Called for I/O errors and for anything that leaves the reply stream out
	/// of sync. Resyncing a stream we have lost our place in is guesswork, and
	/// guessing here means reading one command's reply as another's — which
	/// shows up as listings and existence checks that quietly answer wrong.
	void fail (char const *reason);

	/// \brief Send "command\r\n" and log it.
	bool sendCommand (std::string const &command);

	/// \brief Read one complete response, following multi-line continuations.
	/// \param code Receives the 3-digit status
	/// \param text Receives the response text
	bool readResponse (int &code, std::string &text);

	/// \brief sendCommand() followed by readResponse().
	bool command (std::string const &command, int &code, std::string &text);

	/// \brief Send a command expecting a 2xx reply.
	bool simpleCommand (std::string const &command);

	/// \brief Read one CRLF-terminated line from the control connection.
	bool readLine (std::string &line);

	/// \brief Enter passive mode and connect the data socket.
	bool openPassive (net::Socket &data);

	/// \brief PASV, then send \a command and expect a 1xx reply.
	bool openData (std::string const &command, net::Socket &data);

	net::Socket m_control;
	std::string m_buffer; ///< leftover bytes between control lines
	std::string m_host;
	std::uint16_t m_port = 21;
	bool m_utf8          = false;
	bool m_broken        = false; ///< latched by fail(); only connect() clears it
};
}
