// The front door for every command coming off the wire: parse it, route it,
// and clean up properly when a client goes away.
#pragma once

#include <memory>
#include <string>

#include "network/session.hpp"

// Executes the command received from `session` (a line already read from
// the socket) and writes the response (OK/ERR) plus any broadcast EVTs
// directly to the involved sockets.
void handle_command(std::shared_ptr<Session> session, const std::string& line);

// Call this when a client disconnects (read loop ended): removes the
// player's state and notifies the room (in this order, to avoid a leave
// event referring to already-inconsistent state).
void handle_disconnect(std::shared_ptr<Session> session);
