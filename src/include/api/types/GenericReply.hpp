#pragma once

#include <variant>
#include "api/types/ForceReply.hpp"
#include "api/types/InlineKeyboardMarkup.hpp"
#include "api/types/ReplyKeyboardMarkup.hpp"
#include "api/types/ReplyKeyboardRemove.hpp"

namespace api::types {
	using GenericReply = std::variant<
		api::types::InlineKeyboardMarkup,
		api::types::ReplyKeyboardMarkup,
		api::types::ReplyKeyboardRemove,
		api::types::ForceReply>;
}