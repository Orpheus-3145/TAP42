#pragma once

#include <cstdint>
#include <string>
#include <functional>

#include "TapWindow.hpp"


class ErrorWindow : public TapWindow
{
	public:
		ErrorWindow(void);

		virtual ~ErrorWindow(void) noexcept { this->clear(); }

		void draw(int32_t height, int32_t width) override;
		void clear(void) noexcept override;

		void updateDescription(std::string const& description);
		void setAction1(std::string const& actionName, std::function<void()> action);
		void setAction2(std::string const& actionName, std::function<void()> action);

	protected:
		static constexpr size_t FRAME = 0UL;
		static constexpr size_t INFO = 1UL;
		static constexpr size_t ACTION1 = 2UL;
		static constexpr size_t ACTION2 = 3UL;
};

