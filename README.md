# TAP42
TAP, or The Answer Protocol, is an MUD (multi-user dungeon), or a multiplayer text-adventure game, served on a client-server architecture based on TCP protocol, as specifically defined inside the RFC.42TAP document protocol


# Client

#### Building
From the client/ folder:
	. `make cli` to build the Command Line interface
	. `make gui` to build the Graphic interface

#### Running
`tap-client -i <hostname> -p <port>`

`tap-client -h` for usage

#### Session
- The CLI version is supposed to be played with only with the keyboard, mouse won't be necessary.
- `shared/include/Config.hpp` has some basic customization of the UI, that the minimum size shuldn't be reduced to avoid bugs.
- Logs are stored inside `client/logs`.

##### Keys:
1. `tab` for switching focus (user events - world events - chat events) between tabs (text or buttons) of the window; a highlighted tab will blink repeatedly 
2. `up` and `down` arrows for checking the history of the previous input messages
3. `back-tab` to suggest a command (case-insesitive) from the given input
4. Other keyboard cursor manipulations (`del`, `backspace`, `home`, `end`, `left/right` arrow) work as in a normal terminal
5. Press `ENTER` to send an input or to activate a button

Note that the client will expected the TCP handshake from server before forwarding the username.