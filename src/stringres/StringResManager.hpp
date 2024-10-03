#include <CStringLifetime.h>
#include <TgBotStringResManagerExports.h>
#include <resources.gen.h>

#include <InstanceClassBase.hpp>
#include <utility>

#include "StringResLoader.hpp"

// Shorthand macro for getting a string
#define GETSTR(x) StringResManager::getInstance()->getString(STRINGRES_##x)
#define GETSTR_IS(x) GETSTR(x) + ": "
#define GETSTR_BRACE(x) "(" + GETSTR(x) + ")"

// Implementation of StringResManager (Proxy class)
struct TgBotStringResManager_API StringResManager
    : InstanceClassBase<StringResManager> {
   private:
    std::unique_ptr<StringResLoaderBase> m_loader;

   public:
    explicit StringResManager(std::unique_ptr<StringResLoaderBase> loader)
        : m_loader(std::move(loader)) {}
    [[nodiscard]] std::string getString(const int key) const {
        return std::string{m_loader->getString(key)};
    }
};