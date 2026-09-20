#pragma once

#include "CurseWindow.hpp"
#include "CurseTab.hpp"


class PlayerCreateWindow : public CurseWindow
{
	public:
		PlayerCreateWindow(int32_t commandFd);

		virtual ~PlayerCreateWindow(void) noexcept { this->clear(); }

		void draw(int32_t height, int32_t width) override;
		void clear(void) noexcept override;
		void readInput(void) override;
		void resize(int32_t height, int32_t width) override;
		void switchInputTab(void) noexcept override {}
		void scrollTab(bool goingUp) noexcept override { (void) goingUp; }

	protected:
		int32_t commandFd;

		std::unique_ptr<BasicTab>		tmp, frame;
		std::unique_ptr<OutputTab>		outputNameTab;
		std::unique_ptr<SingleInputTab>	inputNameTab;
};
