#include <algorithm>
#include <array>
#include <utility>

namespace array_helpers {

template <typename T, typename V>
using ArrayElem = std::pair<T, V>;
template <typename T, typename V>
using ConstArrayElem = const ArrayElem<T, V>;
template <typename T, typename V, int size>
using ConstArray = std::array<ConstArrayElem<T, V>, size>;

template <class Container, typename T>
auto find(Container& c, T val) {
    return std::find_if(c.begin(), c.end(), [=](const auto& e) {
        return e.first == val;
    });
}

// T, V : Map elements, N: count, E valargs
template <int N, typename T, typename V, typename... E>
ConstArray<T, V, N> make(E&&... e) {
    static_assert(sizeof...(E) == N, "Must match declared size");
    return {{std::forward<ConstArrayElem<T, V>>(e)...}};
}

template <typename T, typename V>
ArrayElem<T, V> make_elem(T&& t, V&& v) {
    return std::make_pair<T, V>(std::forward<T>(t), std::forward<V>(v));
}

}