#pragma once

#include "fs/entry.h"

#include <cstdint>
#include <string>
#include <vector>

namespace transfer
{
enum Direction
{
	DOWNLOAD,   ///< server -> SD card
	UPLOAD,     ///< SD card -> server
	LOCAL_COPY, ///< SD card -> SD card
};

enum Status
{
	PENDING,
	ACTIVE,
	DONE,
	CANCELLED,
	FAILED,
};

/// What to do when the destination already exists. F4's overwrite prompt sets
/// this per request; a recursive copy therefore asks at most once.
enum Overwrite
{
	OVERWRITE_ASK, ///< resolved by the UI before the request is queued
	OVERWRITE_YES,
	OVERWRITE_NO,
};

/// One file (or one directory to create). Directories carry no bytes; they
/// exist in the list so the destination tree is built in order.
struct Job
{
	unsigned id = 0;
	Direction direction = DOWNLOAD;
	std::string source;
	std::string dest;
	std::string name; ///< what the progress panel shows
	std::uint64_t size = 0;
	std::uint64_t done = 0;
	Status status      = PENDING;
	bool directory     = false;
};

/// A user action, expanded into jobs by the worker. Directories are walked
/// recursively, which needs listings and therefore has to happen off the UI
/// thread.
struct Request
{
	Direction direction = DOWNLOAD;
	std::string source;   ///< file or directory on the source machine
	std::string destDir;  ///< destination directory
	Overwrite overwrite = OVERWRITE_YES;
};

void init ();

/// \brief Queue a request and make sure the worker is draining.
void enqueue (Request request);

/// \brief Stop the worker picking up anything new. The job already running
/// continues; only the queue is held.
///
/// Needed while a prompt about the queue is on screen: otherwise transfers keep
/// starting behind the dialog and the answer applies to a different queue than
/// the one the user was looking at.
void pause ();

/// \brief Undo pause() and get the worker going again if work is waiting.
void resume ();

/// \brief Copy the job list out for rendering.
void snapshot (std::vector<Job> &out);

/// \brief Whether anything is expanding, pending or active.
bool active ();

/// \brief Whether the worker is still walking a directory tree.
bool expanding ();

/// \brief How many files are still waiting, the one in flight included.
///
/// Not the same as counting the job list: a selection of twenty files is
/// twenty requests, and the worker only turns a request into jobs once
/// everything queued ahead of it has run, so most of the backlog has no job
/// yet at all.
///
/// \param atLeast Set when part of the backlog is still unexpanded, which makes
///        the returned number a lower bound (a folder counts as one until it is
///        walked, and it may hold hundreds of files)
int pending (bool &atLeast);

/// \brief Cancel one job by id. An active job is aborted; a pending one is
/// simply marked.
void cancel (unsigned id);

/// \brief Cancel every job that has not started yet.
void cancelPending ();

/// \brief Cancel everything, including the active job. Used when quitting.
void cancelAll ();

/// \brief Throughput of the active job in bytes per second, 0 if idle.
double rate ();

/// \brief Seconds left on the active job, or -1 when it cannot be estimated.
int eta ();

/// \brief Forget finished/cancelled jobs.
void clearFinished ();
}
