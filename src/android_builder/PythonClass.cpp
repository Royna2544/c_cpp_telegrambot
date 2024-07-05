#include "PythonClass.hpp"

#include <absl/log/log.h>

#include <limits>
#include <memory>

namespace details {

template <>
bool convert<bool>(PyObject* value) {
    if (!PyBool_Check(value)) {
        LOG(ERROR) << "Invalid boolean value: expected boolean object";
        return std::numeric_limits<bool>::min();
    }
    return PyObject_IsTrue(value) == 1;
}

template <>
long convert<long>(PyObject* value) {
    if (!PyLong_Check(value)) {
        LOG(ERROR) << "Invalid integer value: expected integer object";
        return std::numeric_limits<long>::min();
    }
    return PyLong_AsLong(value);
}

template <>
double convert<double>(PyObject* value) {
    if (!PyFloat_Check(value)) {
        LOG(ERROR) << "Invalid float value: expected float object";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return PyFloat_AsDouble(value);
}

template <>
std::string convert<std::string>(PyObject* value) {
    Py_ssize_t size{};
    const auto* buf = PyUnicode_AsUTF8AndSize(value, &size);
    if (buf == nullptr) {
        LOG(ERROR) << "Failed to convert Unicode to UTF-8";
        return {};
    }
    return {buf, static_cast<size_t>(size)};
}

template <>
std::vector<long> convert<std::vector<long>>(PyObject* value) {
    std::vector<long> result;
    if (PyList_Check(value)) {
        Py_ssize_t size = PyList_Size(value);
        for (Py_ssize_t i = 0; i < size; ++i) {
            PyObject* item = PyList_GetItem(value, i);
            if (PyLong_Check(item)) {
                result.push_back(details::convert<long>(item));
            } else {
                LOG(ERROR) << "Invalid list item: expected integer";
            }
            Py_DECREF(item);
        }
    } else {
        LOG(ERROR) << "Invalid input: expected list";
    }
    return result;
}
}  // namespace details

bool PythonClass::addLookupDirectory(const std::filesystem::path& directory) {
    // Get the sys.path list from Python interpreter
    PyObject* sysPath = PySys_GetObject("path");

    // Convert the directory path to a Python string
    PyObject* dirStr = PyUnicode_DecodeFSDefault(directory.c_str());

    // Append the directory string to sys.path
    if (PyList_Append(sysPath, dirStr) != 0) {
        LOG(ERROR) << "Failed to append directory to sys.path";
        Py_DECREF(dirStr);
        return false;
    }

    // Decrement the reference count of the directory string
    Py_DECREF(dirStr);
    return true;
}

std::shared_ptr<PythonClass::ModuleHandle> PythonClass::importModule(
    const std::string& name) {
    // Import the specified module
    PyObject* module = PyImport_ImportModule(name.c_str());

    // If the module is not found, throw a runtime_error
    if (module == nullptr) {
        PyErr_Print();
        LOG(ERROR) << "Module not found: " << name;
        return nullptr;
    }
    ModuleHandle tmp(shared_from_this(), module, name);
    return std::make_shared<ModuleHandle>(std::move(tmp));
}

std::shared_ptr<PythonClass::FunctionHandle>
PythonClass::ModuleHandle::lookupFunction(const std::string& name) {
    // Get the specified function from the current module
    PyObject* function = PyObject_GetAttrString(module, name.c_str());

    // If the function is not found, throw a runtime_error
    if (function == nullptr) {
        PyErr_Print();
        LOG(ERROR) << "Function not found: " << name;
        return nullptr;
    }

    // Ensure the function is callable
    if (PyCallable_Check(function) == 0) {
        Py_DECREF(function);
        LOG(ERROR) << "Object is not callable";
        return nullptr;
    }

    FunctionHandle temp { shared_from_this(), function, name };
    return std::make_shared<FunctionHandle>(std::move(temp));
}
