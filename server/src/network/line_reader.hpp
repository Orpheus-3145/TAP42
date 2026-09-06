#pragma once

#include <string>

#include "network/session.hpp"

// Reads one complete line (terminated by '\n') from the session's socket,
// buffering partial or multi-line recv() calls in session.recv_buffer.
// Returns false if the connection is closed or errored.
bool read_line(Session& session, std::string& out_line);
