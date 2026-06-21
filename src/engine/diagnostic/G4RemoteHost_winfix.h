// Windows compatibility helpers for G4RemoteHost.cpp
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <process.h>
#define close closesocket
#define getpid _getpid
inline int setNonblockWin(int sock) { u_long mode=1; return ioctlsocket(sock, FIONBIO, &mode); }
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
inline int setNonblockWin(int) { return 0; }
#endif
