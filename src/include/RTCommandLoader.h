#pragma once

#include <NamespaceImport.h>
#include <string>
#include <filesystem>

void loadOneCommand(Bot& bot, const std::filesystem::path fname);
void loadCommandsFromFile(Bot& bot, const std::filesystem::path filename);
