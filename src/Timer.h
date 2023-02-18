#pragma once

#include <functional>
#include <thread>
#include <atomic>
#include <thread>

struct timehms {
	int h;
	int m;
	int s;
};

using callback_t = std::function<void(void *)>;
using time_callback_t = std::function<void(void *, struct timehms)>;

class Timer {
	unsigned int h, m, s;
	unsigned int onsec;
	void *priv;
	time_callback_t onEvery;
	callback_t onEnd;
	std::atomic_bool stop;

 public:
	Timer() = delete;
	Timer(unsigned int h, unsigned int m, unsigned int s) : h(h), m(m), s(s) {};

	void setCallback(const time_callback_t onEvery, const unsigned int onsec, const callback_t onEnd, const void *priv);
	void start(void);
	void cancel(void);
};
