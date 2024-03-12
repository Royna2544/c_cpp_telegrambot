#include <vector>
#include <command_modules/CommandModule.h>

extern struct CommandModule cmd_addblacklist;
extern struct CommandModule cmd_rmblacklist;
extern struct CommandModule cmd_addwhitelist;
extern struct CommandModule cmd_rmwhitelist;
extern struct CommandModule cmd_bash;
extern struct CommandModule cmd_ubash;
extern struct CommandModule cmd_alive;
extern struct CommandModule cmd_flash;
extern struct CommandModule cmd_possibility;
extern struct CommandModule cmd_decide;
extern struct CommandModule cmd_delay;
extern struct CommandModule cmd_decho;
extern struct CommandModule cmd_randsticker;
extern struct CommandModule cmd_fileid;
extern struct CommandModule cmd_updatecommands;
extern struct CommandModule cmd_ibash;
extern struct CommandModule cmd_starttimer;
extern struct CommandModule cmd_stoptimer;
extern struct CommandModule cmd_saveid;
extern struct CommandModule cmd_start;

inline const std::vector gCmdModules = {
   &cmd_addblacklist,
   &cmd_rmblacklist,
   &cmd_addwhitelist,
   &cmd_rmwhitelist,
   &cmd_bash,
   &cmd_ubash,
   &cmd_alive,
   &cmd_flash,
   &cmd_possibility,
   &cmd_decide,
   &cmd_delay,
   &cmd_decho,
   &cmd_randsticker,
   &cmd_fileid,
   &cmd_updatecommands,
   &cmd_ibash,
   &cmd_starttimer,
   &cmd_stoptimer,
   &cmd_saveid,
   &cmd_start,
};
