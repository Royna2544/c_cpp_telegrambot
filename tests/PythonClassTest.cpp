#include <Python.h>
#include <absl/log/log.h>
#include <gtest/gtest.h>

#include <ArgumentBuilder.hpp>
#include <PythonClass.hpp>
#include <filesystem>
#include <string>

class PythonClassTest : public ::testing::Test {
   protected:
    void SetUp() override {
        PythonClass::init();
        pythonClass = PythonClass::get();
        std::filesystem::path testDir =
            std::filesystem::current_path() / "tests";
        pythonClass->addLookupDirectory(testDir);
    }

    void TearDown() override {
        pythonClass.reset();
        PythonClass::deinit();
    }

    PythonClass::Ptr pythonClass;
};

TEST_F(PythonClassTest, AddLookupDirectory) {
    std::filesystem::path testDir = std::filesystem::current_path();
    ASSERT_TRUE(pythonClass->addLookupDirectory(testDir));
}

TEST_F(PythonClassTest, ImportModule) {
    auto module = pythonClass->importModule("test_module");
    ASSERT_NE(module, nullptr);
}

TEST_F(PythonClassTest, CallSingleFunction) {
    auto module = pythonClass->importModule("test_module");
    ASSERT_NE(module, nullptr);

    auto function = module->lookupFunction("test_function");
    ASSERT_NE(function, nullptr);

    long out;
    ASSERT_TRUE(function->call(nullptr, &out));
    ASSERT_EQ(out, 42);  // Assuming test_function returns 42
}

TEST_F(PythonClassTest, CallMultipleFunctions) {
    auto module = pythonClass->importModule("test_module");
    ASSERT_NE(module, nullptr);

    auto func1 = module->lookupFunction("test_function");
    ASSERT_NE(func1, nullptr);

    auto func2 = module->lookupFunction("add_function");
    ASSERT_NE(func2, nullptr);

    auto func3 = module->lookupFunction("test_function");
    ASSERT_NE(func3, nullptr);

    long out1;
    ASSERT_TRUE(func1->call(nullptr, &out1));
    ASSERT_EQ(out1, 42);

    ArgumentBuilder builder(2);
    auto* arg = builder.add_argument(3).add_argument(4).build();
    ASSERT_NE(arg, nullptr);
    long out2;
    ASSERT_TRUE(func2->call(arg, &out2));
    ASSERT_EQ(out2, 7);  // Assuming add_function(3, 4) returns 7

    Py_DECREF(arg);
}
