#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

template <typename T>
concept HasIsSupportedBool = requires(T t) {
    { t.isSupported() } -> std::same_as<bool>;
};

template <typename Base, typename... Classes>
    requires(std::is_base_of_v<Base, Classes> && ...) &&
            (HasIsSupportedBool<Classes>, ...)

class BackendChooser {
    std::tuple<Classes...> objects;

    void p(Base& obj, Base** out) {
        if (*out != nullptr) {
            // Already set
            return;
        }
        if (obj.isSupported()) {
            onMatchFound(obj);
            *out = &obj;
        } else {
            onNotMatched(obj);
        }
    }

   public:
    Base* getObject() {
        Base* result = nullptr;
        std::apply(
            [this, &result](auto&&... args) { ((p(args, &result)), ...); },
            objects);
        return result;
    }

    /**
     * @brief Virtual method to be overridden by derived classes.
     * This method is called when a matching object is found.
     *
     * @param obj The matching object.
     */
    virtual void onMatchFound(Base& obj) {}

    /**
     * @brief Virtual method to be overridden by derived classes.
     * This method is called when no matching object is found.
     *
     * @param obj The object for which no match was found.
     */
    virtual void onNotMatched(Base& obj) {}

    virtual ~BackendChooser() = default;
};