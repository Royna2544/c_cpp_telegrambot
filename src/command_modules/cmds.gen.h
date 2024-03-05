#include <vector>
#include <command_modules/CommandModule.h>

extern const struct CommandModule cmd_addblacklist;
extern const struct CommandModule cmd_rmblacklist;
extern const struct CommandModule cmd_addwhitelist;
extern const struct CommandModule cmd_rmwhitelist;
extern const struct CommandModule cmd_bash;
extern const struct CommandModule cmd_unsafebash;
extern const struct CommandModule cmd_alive;
extern const struct CommandModule cmd_flash;
extern const struct CommandModule cmd_possibility;
extern const struct CommandModule cmd_decide;
extern const struct CommandModule cmd_delay;
extern const struct CommandModule cmd_decho;
extern const struct CommandModule cmd_randsticker;
extern const struct CommandModule cmd_fileid;
extern const struct CommandModule cmd_starttimer;
extern const struct CommandModule cmd_stoptimer;
extern const struct CommandModule cmd_saveid;
inline const std::vector gCmdModules = {
   &cmd_addblacklist,
   &cmd_rmblacklist,
   &cmd_addwhitelist,
   &cmd_rmwhitelist,
   &cmd_bash,
   &cmd_unsafebash,
   &cmd_alive,
   &cmd_flash,
   &cmd_possibility,
   &cmd_decide,
   &cmd_delay,
   &cmd_decho,
   &cmd_randsticker,
   &cmd_fileid,
   &cmd_starttimer,
   &cmd_stoptimer,
   &cmd_saveid,
};
