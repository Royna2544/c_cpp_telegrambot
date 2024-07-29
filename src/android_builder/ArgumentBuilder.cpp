#include "ArgumentBuilder.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>
#include "PythonClass.hpp"

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
        PythonClass::Py_maybePrintError();
        return nullptr;
    }
    DLOG(INFO) << "Building arguments: size=" << argument_count;
    arguments_ref.reserve(argument_count);
    for (Py_ssize_t i = 0; i < argument_count; ++i) {
        PyObject* value = nullptr;
        std::visit(
            [&value, &arguments_ref, i](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, long>) {
                    value = PyLong_FromLong(arg);
                    arguments_ref.emplace_back(value);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    value = PyUnicode_FromString(arg.c_str());
                    arguments_ref.emplace_back(value);
                }
                DLOG(INFO) << "Arguments[" << i << "]: " << arg;
            },
            arguments[i]);
        if (value == nullptr) {
            LOG(ERROR) << "Failed to convert argument to Python object";
            Py_DECREF(result);
            PythonClass::Py_maybePrintError();
            for (const auto& obj : arguments_ref) {
                Py_XDECREF(obj);
            }
            return nullptr;
        }
        PyTuple_SetItem(result, i, value);
    }
    return result;
}