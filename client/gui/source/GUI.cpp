#include "GUI.hpp"

#include <string>


static int argc = 1;
static char arg0[] = "tap-gui-client";
static char* argv[] = { arg0, nullptr };

GUI::GUI(int32_t clientSocket, int32_t height, int32_t width) :
	UI(clientSocket),
	app(argc, argv),
	mainWindow(nullptr, height, width)
{
    this->mainWindow.show();
}

void GUI::handleCommand(void)
{
}

void GUI::handleResponse(std::string const& response) noexcept
{
	(void) response;
}

void GUI::handleEvent(std::string const& event) noexcept
{
	(void) event;
}

void GUI::refresh(void) noexcept
{
}


std::unique_ptr<UI> uiFactory(int32_t clientSocket)
{
	return std::make_unique<GUI>(clientSocket, Config::HEIGHT_GUI, Config::WIDTH_GUI);
}
