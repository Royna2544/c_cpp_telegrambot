#include "UploadFileTask.hpp"

#include <ArgumentBuilder.hpp>

bool UploadFileTask::runFunction() {
    auto py = PythonClass::get();
    auto mod = py->importModule("upload_file");
    if (!mod) {
        LOG(ERROR) << "Could not import module upload_file";
        return false;
    }
    // Device name, PrefixStr
    auto func = mod->lookupFunction("upload_to_gofile");
    if (!func) {
        LOG(ERROR) << "Could not find function upload_to_gofile";
        return false;
    }
    ArgumentBuilder builder(2);
    builder.add_argument(data.bConfig.device);
    builder.add_argument(data.rConfig.prefixOfOutput);
    std::string resultString;
    if (!func->call(builder.build(), &resultString)) {
        LOG(ERROR) << "Error calling function upload_to_gofile";
        return false;
    }
    LOG(INFO) << resultString;
    data.result->setMessage(resultString);
    return true;
}

void UploadFileTask::onExit(int exitCode) {
    switch (exitCode) {
        case EXIT_SUCCESS:
            data.result->value = PerBuildData::Result::SUCCESS;
            break;
        case EXIT_FAILURE:
            data.result->value = PerBuildData::Result::ERROR_FATAL;
            break;
        default:
            break;
    }
}

UploadFileTask::UploadFileTask(PerBuildData data) : data(std::move(data)) {}
