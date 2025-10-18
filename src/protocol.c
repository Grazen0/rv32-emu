#include "protocol.h"
#include "log.h"
#include "stdinc.h"
#include "str.h"
#include <errno.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

[[nodiscard]] static bool safe_write(const int fd, const void *buf, const size_t count)
{
    const u8 *ptr = buf;
    size_t remaining = count;

    while (remaining > 0) {
        const ssize_t written = write(fd, ptr, remaining);
        if (written < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                continue;

            return false;
        }

        ptr += written;
        remaining -= written;
    }

    return true;
}

/**
 * \brief Reads data from a BufSock and fills its buffer.
 *
 * \param buf_sock The BufSock to read from.
 *
 * \return The result of the operation.
 */
[[nodiscard]] static GdbResult BufSock_refill(BufSock *const buf_sock)
{
    const ssize_t result =
        read(buf_sock->sock, buf_sock->buf, buf_sock->buf_capacity * sizeof(*buf_sock->buf));

    if (result == 0)
        return GdbResult_UnexpectedEof;

    if (result < 0)
        return GdbResult_ReadError;

    buf_sock->buf_size = result;
    buf_sock->buf_head = 0;

    return GdbResult_Ok;
}

[[nodiscard]] static GdbResult BufSock_read_buf(BufSock *const buf_sock, char *const out)
{
    if (buf_sock->buf_head >= buf_sock->buf_size) {
        const GdbResult result = BufSock_refill(buf_sock);
        if (result != GdbResult_Ok)
            return result;
    }

    *out = buf_sock->buf[buf_sock->buf_head];
    ++buf_sock->buf_head;

    return GdbResult_Ok;
}

[[nodiscard]] static GdbResult BufSock_receive_packet(BufSock *const buf_sock, Packet *const dest)
{
    char ch = '\0';

    while (ch != '$') {

        const GdbResult result = BufSock_read_buf(buf_sock, &ch);
        if (result != GdbResult_Ok)
            return result;
    }

    String data = String_new();

    while (true) {
        const GdbResult result = BufSock_read_buf(buf_sock, &ch);
        if (result != GdbResult_Ok) {
            String_destroy(&data);
            return result;
        }

        if (ch == '#')
            break;

        String_push(&data, ch);
    }

    char checksum_str[3] = {};

    GdbResult result = BufSock_read_buf(buf_sock, &checksum_str[0]);
    if (result != GdbResult_Ok) {
        String_destroy(&data);
        return result;
    }

    result = BufSock_read_buf(buf_sock, &checksum_str[1]);
    if (result != GdbResult_Ok) {
        String_destroy(&data);
        return result;
    }

    *dest = (Packet){
        .data = data,
        .checksum = strtol(checksum_str, nullptr, 16),
    };
    return GdbResult_Ok;
}

[[nodiscard]] static GdbResult wait_for_ack(BufSock *const client, const String *const raw_packet)
{
    while (true) {
        char ch = '\0';

        const GdbResult result = BufSock_read_buf(client, &ch);
        if (result != GdbResult_Ok)
            return result;

        if (ch == '+')
            break;

        if (ch == '-') {
            // Resend packet
            if (!safe_write(client->sock, raw_packet->data, raw_packet->size))
                return GdbResult_WriteError;
        }
    }

    return GdbResult_Ok;
}

[[nodiscard]] static GdbResult
GdbServer_send_response(GdbServer *const server, BufSock *const client, const String *const data)
{
    ver_printf("Response: %s\n", data->data);

    const u8 checksum = data_checksum(data);

    String raw_packet = String_with_capacity(data->size + 4);
    String_push(&raw_packet, '$');
    String_push_str(&raw_packet, *data);
    String_push(&raw_packet, '#');
    String_push_hex(&raw_packet, checksum);

    if (!safe_write(client->sock, raw_packet.data, raw_packet.size))
        return GdbResult_WriteError;

    if (!server->no_ack_mode) {
        GdbResult result = wait_for_ack(client, &raw_packet);
        if (result != GdbResult_Ok)
            return result;
    }

    String_destroy(&raw_packet);
    return GdbResult_Ok;
}

char *GdbResult_display(const GdbResult result)
{
    switch (result) {
    case GdbResult_Ok:
        return "Ok.";
    case GdbResult_ReadError:
        return "Error reading socket.";
    case GdbResult_UnexpectedEof:
        return "Client sent an unexpected EOF.";
    case GdbResult_CreateSocketError:
        return "Error creating socket.";
    case GdbResult_BindError:
        return "Error binding socket.";
    case GdbResult_ListenError:
        return "Error listening on address.";
    case GdbResult_WriteError:
        return "Error writing to socket.";
    default:
        return "Invalid result value.";
    }
}

BufSock BufSock_new(const int sock, const size_t buf_capacity)
{
    BufSock buf_sock = {
        .sock = sock,
        .buf = malloc(buf_capacity * sizeof(*buf_sock.buf)),
        .buf_capacity = buf_capacity,
        .buf_size = 0,
        .buf_head = 0,
    };

    return buf_sock;
}

GdbResult BufSock_try_read_buf(BufSock *const buf_sock, char *const out)
{
    if (buf_sock->buf_head >= buf_sock->buf_capacity) {
        struct pollfd pfd = {
            .fd = buf_sock->sock,
            .events = POLLIN,
        };

        const int ret = poll(&pfd, 1, 0);

        if (ret <= 0 || ((pfd.revents & POLLIN) == 0))
            return false;

        const GdbResult refill_result = BufSock_refill(buf_sock);
        if (refill_result != GdbResult_Ok)
            return refill_result;
    }

    *out = buf_sock->buf[buf_sock->buf_head];
    ++buf_sock->buf_head;

    return true;
}

void BufSock_destroy(BufSock *const buf_sock)
{
    free(buf_sock->buf);
    buf_sock->buf = nullptr;
    buf_sock->buf_capacity = 0;
    buf_sock->buf_size = 0;
    buf_sock->buf_head = 0;
}

bool GdbServer_new(const PacketHandler handler, void *const ctx, GdbServer *const out)
{
    const int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
        return false;

    const int yes = true;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    *out = (GdbServer){
        .sock = server_sock,
        .addr = {},
        .listening = false,
        .no_ack_mode = false,
        .handler = handler,
        .ctx = ctx,
        .quit = false,
    };

    return true;
}

bool GdbServer_listen(GdbServer *const server, const u16 port)
{
    static constexpr int MAX_CONNECTIONS = 1;

    server->addr = (struct sockaddr_in){
        .sin_family = AF_INET,
        .sin_addr = {.s_addr = INADDR_ANY},
        .sin_port = htons(port),
    };

    if (bind(server->sock, (struct sockaddr *)&server->addr, sizeof(server->addr)) < 0)
        return false;

    if (listen(server->sock, MAX_CONNECTIONS) < 0)
        return false;

    server->listening = true;
    return true;
}

int GdbServer_accept_connection(GdbServer *const server, struct sockaddr_in *const client_addr,
                                socklen_t *const addr_len)
{
    return accept(server->sock, (struct sockaddr *)client_addr, addr_len);
}

GdbResult GdbServer_handle_client(GdbServer *const server, const int client_sock)
{
    static constexpr size_t CLIENT_BUF_CAPACITY = 128;

    BufSock client = BufSock_new(client_sock, CLIENT_BUF_CAPACITY);

    while (!server->quit) {
        Packet packet = {};
        GdbResult result = BufSock_receive_packet(&client, &packet);

        if (result != GdbResult_Ok) {
            BufSock_destroy(&client);
            return result;
        }

        if (!server->no_ack_mode) {
            const u8 computed_checksum = data_checksum(&packet.data);

            if (computed_checksum != packet.checksum) {
                Packet_destroy(&packet);

                if (!safe_write(client_sock, "-", 1)) // NACK
                    return GdbResult_WriteError;

                continue;
            }
        }

        ver_printf("=======================================\n");
        ver_printf("Packet: '%s'\n", packet.data.data);

        if (!server->no_ack_mode)
            if (!safe_write(client_sock, "+", 1)) // NACK
                return GdbResult_WriteError;

        String response_data = server->handler(server->ctx, &packet, server, &client);
        result = GdbServer_send_response(server, &client, &response_data);

        Packet_destroy(&packet);
        String_destroy(&response_data);

        if (result != GdbResult_Ok) {
            BufSock_destroy(&client);
            return result;
        }
    }

    close(client.sock);
    BufSock_destroy(&client);
    return GdbResult_Ok;
}

void GdbServer_set_no_ack_mode(GdbServer *const server, const bool no_ack_mode)
{
    server->no_ack_mode = no_ack_mode;
}

void GdbServer_destroy(GdbServer *const server)
{
    if (!server->listening)
        return;

    server->listening = false;
    close(server->sock);
}

void Packet_destroy(Packet *const packet)
{
    String_destroy(&packet->data);
    packet->checksum = 0;
}

u8 data_checksum(const String *const s)
{
    u8 checksum = 0;

    for (size_t i = 0; i < s->size; ++i)
        checksum += (u8)s->data[i];

    return checksum;
}
