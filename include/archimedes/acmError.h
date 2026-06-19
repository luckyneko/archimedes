#pragma once

#include <string>

namespace acm
{
	class Error
	{
	public:
		Error() = default;
		explicit Error(std::string msg) : m(std::move(msg)) {}

		explicit operator bool() const { return !m.empty(); }
		bool ok() const { return m.empty(); }
		const std::string& message() const { return m; }

	private:
		std::string m;
	};
} // namespace acm
