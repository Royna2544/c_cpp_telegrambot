#include <winsock2.h>
#include <afunix.h>

int main()
{
    (void)sizeof(((struct sockaddr_un *)0)->sun_path);
    return 0;
}