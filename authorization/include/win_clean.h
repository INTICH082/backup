// win_clean.h - Windows-specific cleanup utilities

#ifndef WIN_CLEAN_H
#define WIN_CLEAN_H

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

// Function to clean up Windows sockets and resources
inline void win_cleanup() {
    WSACleanup();
}

// Function to close socket with Windows-specific function
inline void close_socket(SOCKET sock) {
    closesocket(sock);
}

#else
// Unix/Linux compatible stubs
#include <unistd.h>
#include <sys/socket.h>

inline void win_cleanup() {
    // Nothing to clean up on Unix
}

inline void close_socket(int sock) {
    close(sock);
}

#endif // _WIN32

#endif // WIN_CLEAN_H