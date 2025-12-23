#include "SpdlogInit.hpp"
#include <AbslLogCompat.hpp>

extern int app_main(int argc, char** argv);
int main(int argc, char** argv) {
   TgBot_SpdlogInit();
   SPDLOG_INFO("Launching {} with {} args", argv[0], argc);
   return app_main(argc, argv);
}
