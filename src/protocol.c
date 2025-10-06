#include "protocol.h"
#include "netinet/in.h"
#include "str.h"
#include "sys/poll.h"
#include "sys/socket.h"
#include "unistd.h"
#include "util.h"

static constexpr int MAX_CONNECTIONS = 1;

static BufSock BufSock_new(const int sock)
{
    return (BufSock){
        .sock = sock,
        .buf = {},
        .buf_size = 0,
        .buf_head = 0,
    };
}

static void BufSock_refill(BufSock *const reader)
{
    reader->buf_size = read(reader->sock, reader->buf, BUF_READER_CAPACITY * sizeof(char));
    reader->buf_head = 0;
}

[[nodiscard]] static bool BufSock_read_buf(BufSock *const reader, char *const out)
{
    if (reader->buf_head >= reader->buf_size) {
        BufSock_refill(reader);

        if (reader->buf_size == 0)
            return false;
    }

    *out = reader->buf[reader->buf_head];
    ++reader->buf_head;

    return true;
}

[[nodiscard]] static bool BufSock_receive_packet(BufSock *const reader, Packet *const dest)
{
    String data = String_new();
    char ch = '\0';

    while (ch != '$') {
        if (!BufSock_read_buf(reader, &ch))
            return false;
    }

    while (true) {
        if (!BufSock_read_buf(reader, &ch))
            return false;

        if (ch == '#')
            break;

        String_push(&data, ch);
    }

    char checksum_str[3] = {};

    if (!BufSock_read_buf(reader, &checksum_str[0]))
        return false;

    if (!BufSock_read_buf(reader, &checksum_str[1]))
        return false;

    *dest = (Packet){
        .data = data,
        .checksum = strtol(checksum_str, nullptr, 16),
    };
    return true;
}

/**
 * Takes ownership of data.
 */
[[nodiscard]] static bool GdbServer_send_response(GdbServer *const server, BufSock *const client,
                                                  const String *const data)
{
    ver_printf("Response: %s\n", data->data);

    const u8 checksum = data_checksum(data);

    String raw_data = String_with_capacity(data->size + 4);
    String_push(&raw_data, '$');
    String_push_str(&raw_data, *data);
    String_push(&raw_data, '#');
    String_push_hex(&raw_data, checksum);

    write(client->sock, raw_data.data, raw_data.size);

    if (!server->no_ack_mode) {
        while (true) {
            char ch = '\0';

            if (!BufSock_read_buf(client, &ch))
                return false;

            if (ch == '+')
                break;

            if (ch == '-')
                write(client->sock, raw_data.data, raw_data.size);
        }
    }

    String_destroy(&raw_data);
    return true;
}

bool GdbServer_new(const PacketHandler handler, void *const ctx, GdbServer *const out)
{
    const int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
        return false;

    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, nullptr, sizeof(int));

    *out = (GdbServer){
        .sock = server_sock,
        .addr = {},
        .listening = false,
        .no_ack_mode = false,
        .handler = handler,
        .ctx = ctx,
    };

    return true;
}

bool GdbServer_listen(GdbServer *const server, const u16 port)
{
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

bool GdbServer_run(GdbServer *const server, const int client_sock)
{
    BufSock client = BufSock_new(client_sock);

    while (true) {
        Packet packet = {};

        if (!BufSock_receive_packet(&client, &packet))
            return false;

        if (!server->no_ack_mode) {
            const u8 computed_checksum = data_checksum(&packet.data);

            if (computed_checksum != packet.checksum) {
                Packet_destroy(&packet);
                write(client_sock, "-", 1); // NACK
                continue;
            }
        }

        ver_printf("=======================================\n");
        ver_printf("Packet: '%s'\n", packet.data.data);

        if (!server->no_ack_mode)
            write(client_sock, "+", 1); // ACK

        String response_data = server->handler(server->ctx, &packet, server, &client);

        if (!GdbServer_send_response(server, &client, &response_data)) {
            String_destroy(&response_data);
            return EXIT_FAILURE;
        }

        String_destroy(&response_data);
        Packet_destroy(&packet);
    }

    return true;
}

void GdbServer_set_no_ack_mode(GdbServer *const server, const bool no_ack_mode)
{
    server->no_ack_mode = no_ack_mode;
}

bool BufSock_try_read_buf(BufSock *const reader, char *const out)
{
    if (reader->buf_head >= BUF_READER_CAPACITY) {
        struct pollfd pfd = {
            .fd = reader->sock,
            .events = POLLIN,
        };

        const int ret = poll(&pfd, 1, 0);

        if (ret <= 0 || ((pfd.revents & POLLIN) == 0))
            return false;

        read(reader->sock, out, sizeof(*out));

        return true;
    }

    *out = reader->buf[reader->buf_head];
    ++reader->buf_head;

    return true;
}

void GdbServer_destroy(GdbServer *const server)
{
    if (!server->listening)
        return;

    server->listening = false;
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
