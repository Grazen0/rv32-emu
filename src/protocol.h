#ifndef RV32_EMU_PROTOCOL_H
#define RV32_EMU_PROTOCOL_H

#include "stdinc.h"
#include "str.h"
#include <netinet/in.h>
#include <stddef.h>
#include <sys/socket.h>

typedef struct Packet {
    String data;
    u8 checksum;
} Packet;

typedef struct BufSock {
    int sock;
    char *buf;
    size_t buf_capacity;
    size_t buf_size;
    size_t buf_head;
} BufSock;

typedef enum GdbResult : u8 {
    GdbResult_Ok = 0,
    GdbResult_UnexpectedEof,
    GdbResult_ReadError,
    GdbResult_CreateSocketError,
    GdbResult_BindError,
    GdbResult_ListenError,
    GdbResult_WriteError,
} GdbResult;

typedef struct GdbServer GdbServer;

typedef String (*PacketHandler)(void *ctx, const Packet *const packet, GdbServer *server,
                                BufSock *client);

struct GdbServer {
    int sock;
    struct sockaddr_in addr;
    bool listening;
    bool no_ack_mode;
    PacketHandler handler;
    void *ctx;
    bool quit;
};

char *GdbResult_display(GdbResult result);

[[nodiscard]] BufSock BufSock_new(int sock, size_t buf_capacity);

void BufSock_destroy(BufSock *buf_sock);

[[nodiscard]] GdbResult BufSock_try_read_buf(BufSock *buf_sock, char *out);

/**
 * \brief Initializes a new GdbServer.
 *
 * \param handler Pointer to the packet handler function.
 * \param ctx Argument to pass to handler.
 * \param out Pointer to the resulting GdbServer.
 *
 * \return true if successful, false otherwise. If false, errno will be set.
 */
[[nodiscard]] bool GdbServer_new(PacketHandler handler, void *ctx, GdbServer *out);

/**
 * \brief Sets a GdbServer to listen on a port.
 *
 * \param server GdbServer to start listening
 * \param port Port to listen on.
 *
 * \return true if successful, false otherwise. If false, errno will be set.
 */
[[nodiscard]] bool GdbServer_listen(GdbServer *server, u16 port);

/**
 * \brief Accepts the next connection to a GdbServer.
 *
 * Will block until a connection is accepted or an error occurs.
 *
 * \param server The GdbServer to accept from.
 * \param client_addr Will be set address of the accepted client.
 * \param addr_len Will be set to the size of client_addr.
 *
 * \return The accepted connection if successful, -1 otherwise. If -1, errno will be set.
 */
[[nodiscard]] int GdbServer_accept_connection(GdbServer *server, struct sockaddr_in *client_addr,
                                              socklen_t *addr_len);

[[nodiscard]] GdbResult GdbServer_handle_client(GdbServer *server, int client_sock);

void GdbServer_set_no_ack_mode(GdbServer *server, bool no_ack_mode);

void GdbServer_destroy(GdbServer *server);

[[nodiscard]] u8 data_checksum(const String *s);

void Packet_destroy(Packet *packet);

#endif
