#pragma once

#include <stdexcept>
#include <string>

namespace api::types {

/*
 * @brief Base exception class for API-related errors.
 * The implmentations using third-party libraries must throw exceptions
 * derived from this class to ensure consistent error handling.
 */
struct ApiException : public std::runtime_error {
	explicit ApiException(const std::string& message)
		: std::runtime_error(message) {}
};

}  // namespace api::types