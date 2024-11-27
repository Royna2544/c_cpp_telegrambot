#pragma once

#include <concepts>
#include "SystemInfo.hpp"
#include <Progress.hpp>

template <typename T>
concept hasUsageFleid = requires(T t) {
    { t.usage } -> std::same_as<Percent&>;
};

template <hasUsageFleid T, typename... Args>
std::string getPercent(Args&&... args) {
    T percent(std::forward<Args&&>(args...)...);
    return progressbar::create(percent.usage.value);
}