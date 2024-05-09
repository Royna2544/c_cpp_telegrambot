#pragma once

#include "SocketBase.hpp"

/**
 * Returns the appropriate socket implementation based on the current platform
 *
 * @return SocketInterfaceBase * pointer to the socket implementation
 */
SocketInterfaceBase* getClientBackend();
