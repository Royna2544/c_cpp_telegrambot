#pragma once

#include <NamespaceImport.h>

void startTimer(const Bot &bot, const Message::Ptr &message);
void stopTimer(const Bot &bot, const Message::Ptr &message);
void forceStopTimer(void);
