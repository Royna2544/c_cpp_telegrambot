#include "ArgumentBuilder.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>

ArgumentBuilder::ArgumentBuilder(Py_ssize_t argument_count)
    : argument_count(argument_count) {
    arguments.reserve(argument_count);
}

ArgumentBuilder& ArgumentBuilder::add_argument(Variants value) {
    arguments.emplace_back(value);
    CHECK(argument_count >= arguments.size())
        << "Too many arguments, expected " << argument_count;
    return *this;
}

PyObject* ArgumentBuilder::build() {
    if (arguments.size() != argument_count) {
        LOG(ERROR) << "Too few arguments, expected " << argument_count;
        return nullptr;
    }
    PyObject* result = PyTuple_New(argument_count);
    std::vector<PyObject*> arguments_ref;

    if (result == nullptr) {
        LOG(ERROR) << "Failed to create tuple";
        PyErr_Print();
        return nullptr;
    }
    arguments_ref.reserve(argument_count);
    for (Py_ssize_t i = 0; i < argument_count; ++i) {
        PyObject* value = nullptr;
        std::visit(
            [&value, &arguments_ref](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, long>) {
                    value = PyLong_FromLong(arg);
                    arguments_ref.emplace_back(value);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    value = PyUnicode_FromString(arg.c_str());
                    arguments_ref.emplace_back(value);
                }
            },
            arguments[i]);
        if (value == nullptr) {
            LOG(ERROR) << "Failed to convert argument to Python object";
            Py_DECREF(result);
            PyErr_Print();
            for (const auto& obj : arguments_ref) {
                Py_XDECREF(obj);
            }
            return nullptr;
        }
        PyTuple_SetItem(result, i, value);
    }
    return result;
}