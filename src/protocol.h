#ifndef RV32_EMU_PROTOCOL_H
#define RV32_EMU_PROTOCOL_H

#include "netinet/in.h"
#include "stdinc.h"
#include "str.h"
#include "sys/socket.h"

static constexpr size_t BUF_READER_CAPACITY = 128;

typedef struct Packet {
    String data;
    u8 checksum;
} Packet;

typedef struct BufSock {
    int sock;
    char buf[BUF_READER_CAPACITY];
    size_t buf_size;
    size_t buf_head;
} BufSock;

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
};

[[nodiscard]] bool BufSock_try_read_buf(BufSock *reader, char *out);

[[nodiscard]] bool GdbServer_new(PacketHandler handler, void *ctx, GdbServer *out);

[[nodiscard]] bool GdbServer_listen(GdbServer *server, u16 port);

[[nodiscard]] int GdbServer_accept_connection(GdbServer *server, struct sockaddr_in *client_addr,
                                              socklen_t *addr_len);

[[nodiscard]] bool GdbServer_run(GdbServer *server, int client_sock);

void GdbServer_set_no_ack_mode(GdbServer *server, bool no_ack_mode);

void GdbServer_destroy(GdbServer *server);

[[nodiscard]] u8 data_checksum(const String *s);

void Packet_destroy(Packet *packet);

#endif
