#include <spdlog/spdlog.h>

extern int app_main(int argc, char** argv);
int main(int argc, char** argv) {
#ifndef NDEBUG
   spdlog::set_level(spdlog::level::debug);
#endif
   spdlog::info("Launching {} with {} args", argv[0], argc);
   return app_main(argc, argv);
}
