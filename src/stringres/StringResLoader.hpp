#include <CStringLifetime.h>
#include <ConfigManager.h>

#include <filesystem>
#include <string>
#include <vector>

struct StringResLoaderBase {
    virtual ~StringResLoaderBase() = default;

    // key: One of STRINGRES_* constants
    virtual std::string_view getString(const int key) const = 0;
};

class StringResLoader : public StringResLoaderBase {
    std::vector<std::pair<std::string, std::string>> m_strings;

    class Iterator {
        using cit =
            std::vector<std::pair<std::string, std::string>>::const_iterator;

        cit _begin, _end;

       public:
        explicit Iterator(
            const std::vector<std::pair<std::string, std::string>>& strings) {
            _begin = strings.begin();
            _end = strings.end();
        }

        [[nodiscard]] auto begin() const { return _begin; }
        [[nodiscard]] auto end() const { return _end; }
    };

   public:
    [[nodiscard]] Iterator strings() const noexcept {
        return Iterator{m_strings};
    }
    [[nodiscard]] size_t size() const noexcept {
        return m_strings.size();
    }
    bool parse(const std::filesystem::path& path, int expected_size = 0);
    // One of STRINGRES_* constants
    [[nodiscard]] std::string_view getString(const int key) const override;
};