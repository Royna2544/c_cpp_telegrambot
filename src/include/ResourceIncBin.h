#include "third-party/incbin/incbin.h"

#include <string>

INCTXT_EXTERN(AboutHtmlText);
INCTXT_EXTERN(FlashTxt);
INCTXT_EXTERN(MimeDataJson);

#define _TO_INCBIN_SYM_DATA(name)                \
    INCBIN_CONCATENATE(                          \
        INCBIN_CONCATENATE(INCBIN_PREFIX, name), \
        INCBIN_STYLE_IDENT(DATA))

#define _TO_INCBIN_SYM_SIZE(name)                \
    INCBIN_CONCATENATE(                          \
        INCBIN_CONCATENATE(INCBIN_PREFIX, name), \
        INCBIN_STYLE_IDENT(SIZE))

#define ASSIGN_INCTXT_DATA(name, stringval) \
    stringval = std::string(_TO_INCBIN_SYM_DATA(name), _TO_INCBIN_SYM_SIZE(name))
