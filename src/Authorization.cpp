#include <Authorization.h>
#include <Database.h>

bool gAuthorized = true;

static bool AuthorizedId(const int64_t id, const bool permissive) {
#ifdef USE_DATABASE
    static struct config_data data;
    database::config.loadFromFile(&data);
    if (!permissive) {
        for (int i = 0; i < DATABASE_LIST_BUFSIZE; ++i) {
            if (data.whitelist[i] == id) return true;
        }
        return id == data.owner_id;
    } else {
        for (int i = 0; i < DATABASE_LIST_BUFSIZE; ++i) {
            if (data.blacklist[i] == id) return false;
        }
        return true;
    }
#else
    return permissive ? true : ownerid == id;
#endif
}
bool Authorized(const Message::Ptr &message, const bool nonuserallowed, const bool permissive) {
    if (!gAuthorized) return false;
    return message->from ? AuthorizedId(message->from->id, permissive)
                         : nonuserallowed;
}
