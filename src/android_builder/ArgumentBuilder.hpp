#pragma once

#include <Python.h>
#include <string>
#include <variant>
#include <vector>

class ArgumentBuilder {
   public:
    using Variants = std::variant<long, std::string>;

    /**
     * @brief Constructs an ArgumentBuilder
     * object with the specified argument count.
     *
     * @param argument_count The number of
     * arguments to be added to the builder.
     */
    explicit ArgumentBuilder(Py_ssize_t argument_count);

    /**
     * @brief Adds a variant argument to the builder.
     *
     * @param value The variant argument to be added.
     *
     * @return Reference to the builder for method chaining.
     */
    ArgumentBuilder& add_argument(Variants value);

    /**
     * @brief Builds and returns the constructed arguments as a Python object.
     *
     * @return A Python object containing the constructed arguments.
     */
    PyObject* build();

   private:
    std::vector<Variants> arguments;
    Py_ssize_t argument_count = 0;
};