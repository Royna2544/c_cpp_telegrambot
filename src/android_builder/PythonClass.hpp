#pragma once

#include <Python.h>
#include <absl/log/log.h>

#include <filesystem>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace details {

template <typename T>
T convert(PyObject* value) = delete;

template <>
bool convert<bool>(PyObject* value);
template <>
long convert<long>(PyObject* value);
template <>
double convert<double>(PyObject* value);
template <>
std::string convert<std::string>(PyObject* value);
template <>
std::vector<int> convert<std::vector<int>>(PyObject* value);

}  // namespace details

struct GILStateManagement {
    GILStateManagement() : gstate(PyGILState_Ensure()) {}
    ~GILStateManagement() { PyGILState_Release(gstate); }
    PyGILState_STATE gstate;
};

class PythonClass : public std::enable_shared_from_this<PythonClass> {
    // Declare a class to hold Py_init functions
    struct PyInitHolder {
        PyInitHolder() { Py_Initialize(); }
        ~PyInitHolder() { Py_Finalize(); }
    };
    explicit PythonClass() = default;

   public:
    using Ptr = std::shared_ptr<PythonClass>;
    static std::shared_ptr<PythonClass> get() {
        static PyInitHolder holder;
        static auto instance = std::make_shared<PythonClass>(PythonClass());
        return instance;
    }

    class ModuleHandle;

    // Represents a Python function
    class FunctionHandle {
        explicit FunctionHandle(std::shared_ptr<ModuleHandle> module,
                                PyObject* function, std::string name)
            : function(function),
              moduleHandle(std::move(module)),
              name(std::move(name)) {}
        PyObject* function;
        // Need to have a reference to module handle
        std::shared_ptr<ModuleHandle> moduleHandle;

       public:
        friend class ModuleHandle;
        using Ptr = std::shared_ptr<FunctionHandle>;
        std::string name;  // Logging purposes
        ~FunctionHandle() { Py_XDECREF(function); }
        FunctionHandle(FunctionHandle&& other) noexcept
            : function(other.function),
              moduleHandle(std::move(other.moduleHandle)),
              name(std::move(other.name)) {
            other.function = nullptr;
        }

        /**
         * Calls the specified Python function with the given arguments.
         *
         * @param args The arguments to be passed to the Python function.
         * @param out A pointer to the variable where the result will be stored.
         *            If the function returns a value, it will be converted to
         * the specified type and stored in out. If the function does not return
         * a value, out can be nullptr.
         *
         * @return true If the function call is successful and no error occurs.
         *         false If an error occurs during the function call.
         *
         * @note The caller is responsible for managing the lifetime of the args
         * parameter.
         *
         * @note If an error occurs during the function call, the error message
         * will be printed to the standard error stream. The caller can retrieve
         * the error message using PyErr_Print() or PyErr_Fetch().
         *
         * @see https://docs.python.org/3/c-api/object.html#c.Py_DECREF
         * @see https://docs.python.org/3/c-api/exceptions.html#c.PyErr_Print
         * @see https://docs.python.org/3/c-api/exceptions.html#c.PyErr_Fetch
         */
        template <typename T>
        bool call(PyObject* args, T* out) {
            GILStateManagement _;
            LOG(INFO) << "Calling Python function: module "
                      << moduleHandle->name << " function: " << name;

            // Call the specified function with the given arguments
            PyObject* result = PyObject_CallObject(function, args);

            // If an error occurs...
            if (result == nullptr) {
                PyErr_Print();
                LOG(ERROR) << "Failed to call function";
                return false;
            }
            // If the function returned a value, convert it to the specified
            // type and store it in out.
            if constexpr (!std::is_void_v<T>) {
                T resultC = details::convert<T>(result);
                if (PyErr_Occurred()) {
                    PyErr_Print();
                } else {
                    *out = resultC;
                }
            }
            DLOG(INFO) << "Call succeeded";
            // Decrement the reference count of the result
            Py_DECREF(result);
            return true;
        }
    };

    // Represents a Python module
    class ModuleHandle : public std::enable_shared_from_this<ModuleHandle> {
        explicit ModuleHandle(std::shared_ptr<PythonClass> parent,
                              PyObject* module, std::string name)
            : module(module),
              parent(std::move(parent)),
              name(std::move(name)) {}
        PyObject* module;
        // Need to have reference to this too.
        std::shared_ptr<PythonClass> parent;

       public:
        ModuleHandle(ModuleHandle&& other) noexcept
            : module(other.module),
              parent(std::move(other.parent)),
              name(std::move(other.name)) {
            other.module = nullptr;
            other.parent = nullptr;
            other.name.clear();
        }
        std::string name;  // Logging purposes
        ~ModuleHandle() { Py_XDECREF(module); }
        friend class PythonClass;

        std::shared_ptr<FunctionHandle> lookupFunction(const std::string& name);
    };

    /**
     * Adds a directory to the Python's sys.path for module lookup.
     *
     * @param directory The path of the directory to be added.
     *
     * @note This function should be called before importing any modules that
     * are located in the specified directory.
     *
     * @return false If the Python interpreter is not initialized
     * or if an error occurs while appending the directory to sys.path.
     *
     * @see https://docs.python.org/3/library/sys.html#sys.path
     */
    bool addLookupDirectory(const std::filesystem::path& directory);
    std::shared_ptr<ModuleHandle> importModule(const std::string& name);
};