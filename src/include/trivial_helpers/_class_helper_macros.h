#pragma once

#define NO_COPY_ASSIGN(name) \
    name& operator=(const name&) = delete
    
#define NO_DEFAULT_CTOR(name) \
    name() = delete

#define NO_COPY_CTOR(name) \
    name(const name&) = delete; \
    NO_COPY_ASSIGN(name)

#define NO_MOVE_CTOR(name) \
    name(name&&) = delete; \
    name& operator=(name&&) = delete
