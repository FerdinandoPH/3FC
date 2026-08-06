#pragma once

#include "config/config.h"
#include "ftp/ftpClient.h"
#include "fs/entry.h"

#include <functional>
#include <string>

namespace session
{
enum State
{
	DISCONNECTED,
	CONNECTING,
	CONNECTED,
	FAILED, ///< the connection attempt itself did not succeed
	LOST,   ///< we were connected and the control connection died under us
};

/// A unit of work for the network thread. Runs in submission order.
using Task = std::function<void (ftp::Client &)>;

/// \brief Start the network worker.
///
/// It lives on core 1 (enabled by APT_SetAppCpuTimeLimit in platform::init) so
/// blocking socket calls never stall the 60fps UI on core 0.
bool init ();
void exit ();

/// \brief Queue work for the network thread.
void post (Task task);

/// \brief Whether the worker has anything queued or in flight.
bool busy ();

State state ();

/// \brief Message explaining the last failure, if any.
std::string error ();

/// \brief Connect asynchronously; watch state() for the outcome.
void connect (config::Slot const &slot);

/// \brief Disconnect asynchronously and drop cached listings.
void disconnect ();

/// \brief Host:port of the current session, for the status bar.
std::string description ();

/// Remote browsing. The worker fills these in; the UI reads copies.
void requestList (std::string path);

/// \brief Copy the cached remote listing out.
/// \param path Receives the directory the listing belongs to
void listing (std::string &path, fs::Listing &out);

/// \brief Whether a listing request is still outstanding.
bool listPending ();

/// \brief Bumped every time a new listing lands, so the UI can copy it once
/// instead of every frame.
unsigned listGeneration ();
}
