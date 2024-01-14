#pragma once

#include <NamespaceImport.h>
#include <string>

void loadOneCommand(Bot& bot, const std::string& fname);
void loadCommandsFromFile(Bot& bot, const std::string& filename);
