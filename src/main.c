#include "cpu.h"
#include "stdinc.h"
#include <argparse.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

static bool verbose = false;

static constexpr int DEFAULT_PORT = 1234;

static const char *const usages[] = {
    "rv32-emu [options] [--] <filename>",
    nullptr,
};

static int server_sock = 0;
static int client_sock = 0;

static void ver_printf(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

static void cleanup(void)
{
    printf("Shutting down gracefully...\n");
    close(client_sock);
    close(server_sock);
}

static struct sigaction old_action;

static void sigint_handler([[maybe_unused]] const int sig_no)
{
    cleanup();
    sigaction(SIGINT, &old_action, nullptr);
    kill(0, SIGINT);
}

typedef struct GdbPacket {
    char *data;
    size_t data_size;
    u8 checksum;
} GdbPacket;

u8 data_checksum(const char *const data, const size_t size)
{
    u8 checksum = 0;

    for (size_t i = 0; i < size; ++i)
        checksum += (u8)data[i];

    return checksum;
}

static constexpr size_t BUF_CAPACITY = 4096;
static char packet_buf[BUF_CAPACITY];
static size_t packet_buf_size = 0;
static size_t packet_buf_head = 0;

void read_to_buf(const int sock)
{
    packet_buf_size = read(sock, packet_buf, BUF_CAPACITY * sizeof(char));
    packet_buf_head = 0;
}

bool read_buf(const int sock, char *const dest)
{
    if (packet_buf_head >= packet_buf_size) {
        read_to_buf(sock);

        if (packet_buf_size == 0)
            return false;
    }

    *dest = packet_buf[packet_buf_head];
    ++packet_buf_head;

    return true;
}

bool try_read_buf(const int sock, char *const dest)
{
    if (packet_buf_head >= packet_buf_size) {
        struct pollfd pfd = {
            .fd = sock,
            .events = POLLIN,
        };

        const int ret = poll(&pfd, 1, 0);

        if (ret <= 0 || ((pfd.revents & POLLIN) == 0))
            return false;

        read(sock, dest, sizeof(*dest));

        return true;
    }

    *dest = packet_buf[packet_buf_head];
    ++packet_buf_head;

    return true;
}

u8 hex_digit(const char c)
{
    if ('0' <= c && c <= '9')
        return c - '0';

    if ('a' <= c && c <= 'f')
        return 10 + (c - 'a');

    if ('A' <= c && c <= 'F')
        return 10 + (c - 'A');

    return 0xFF;
}

void GdbPacket_destroy(GdbPacket *const self)
{
    free(self->data);
    self->data_size = 0;
    self->checksum = 0;
}

bool receive_packet(const int sock, GdbPacket *const dest)
{
    *dest = (GdbPacket){
        .data = nullptr,
        .data_size = 0,
        .checksum = 0,
    };

    char ch = '\0';
    while (ch != '$') {
        if (!read_buf(sock, &ch))
            return false;
    }

    size_t capacity = 8;

    dest->data_size = 0;
    dest->data = malloc(capacity * sizeof(char));

    while (true) {
        if (!read_buf(sock, &ch))
            return false;

        if (ch == '#')
            break;

        if (dest->data_size == capacity) {
            const size_t new_capacity = 2 * capacity;
            char *const new_data = realloc(dest->data, new_capacity);

            if (new_data == nullptr) {
                free(dest->data);
                dest->data = nullptr;
                return false;
            }

            dest->data = new_data;
            capacity = new_capacity;
        }

        dest->data[dest->data_size] = ch;
        ++dest->data_size;
    }

    char checksum_str[3] = {};

    if (!read_buf(sock, &checksum_str[0]))
        return false;

    if (!read_buf(sock, &checksum_str[1]))
        return false;

    dest->checksum = strtol(checksum_str, nullptr, 16);
    return true;
}

typedef struct Context {
    Cpu cpu;
    int client_sock;
    bool no_ack_mode;
    bool running;
} Context;

bool send_packet(Context *const ctx, const char *const data, const size_t data_size)
{
    ver_printf("Response: %s\n", data);

    const u8 checksum = data_checksum(data, data_size);

    const size_t buf_size = data_size + 4;
    char *buf = malloc((buf_size + 1) * sizeof(*buf));

    memset(buf, 0, (buf_size + 1) * sizeof(*buf));

    buf[0] = '$';
    memcpy(buf + 1, data, data_size * sizeof(*data));
    buf[buf_size - 3] = '#';
    sprintf(buf + buf_size - 2, "%02x", checksum);

    write(ctx->client_sock, buf, buf_size);

    if (!ctx->no_ack_mode) {
        while (true) {
            char ch = '\0';

            if (!read_buf(ctx->client_sock, &ch))
                return false;

            if (ch == '+')
                break;

            if (ch == '-')
                write(ctx->client_sock, buf, buf_size);
        }
    }

    free(buf);
    buf = nullptr;
    return true;
}

bool handle_q_packet(Context *const ctx, const GdbPacket *const packet)
{
    if (strncmp(packet->data, "qSupported", strlen("qSupported")) == 0) {
        static const char *const RES = "QStartNoAckMode+";
        return send_packet(ctx, RES, strlen(RES));
    }

    if (strcmp(packet->data, "QStartNoAckMode") == 0) {
        ctx->no_ack_mode = true;
        static const char *const RES = "OK";
        return send_packet(ctx, RES, strlen(RES));
    }

    if (strcmp(packet->data, "qfThreadInfo") == 0) {
        static const char *const RES = "m1";
        return send_packet(ctx, RES, strlen(RES));
    }

    if (strcmp(packet->data, "qsThreadInfo") == 0) {
        static const char *const RES = "l";
        return send_packet(ctx, RES, strlen(RES));
    }

    if (strcmp(packet->data, "qC") == 0) {
        static const char *const RES = "QC1";
        return send_packet(ctx, RES, strlen(RES));
    }

    if (strcmp(packet->data, "qTStatus") == 0) {
        return send_packet(ctx, "", 0);
    }

    return send_packet(ctx, "", 0);
}

bool handle_v_packet(Context *const ctx, const GdbPacket *const packet)
{
    if (strcmp(packet->data, "vCont?") == 0) {
        static const char *const RES = "vCont;c;s;t";
        return send_packet(ctx, RES, strlen(RES));
    }

    return send_packet(ctx, "", 0);
}

static void append_register_hex(char *const buf, size_t *const pos, u32 value)
{
    for (size_t i = 0; i < 4; ++i) {
        const u8 byte = (value >> (8 * i)) & 0xFF;
        sprintf(buf + *pos, "%02X", byte);
        *pos += 2;
    }
}

bool handle_packet(Context *const ctx, const GdbPacket *const packet)
{
    if (packet->data_size == 0)
        return false;

    if (packet->data[0] == 'q' || packet->data[0] == 'Q')
        return handle_q_packet(ctx, packet);

    if (packet->data[0] == 'v')
        return handle_v_packet(ctx, packet);

    if (packet->data[0] == '?') {
        static const char *const RES = "S05";
        return send_packet(ctx, RES, strlen(RES));
    }

    if (packet->data[0] == 's') {
        Cpu_step(&ctx->cpu);
        static const char *const RES = "S05";
        return send_packet(ctx, RES, strlen(RES));
    }

    if (packet->data[0] == 'c') {
        ctx->running = true;
        return true;
    }

    if (strncmp(packet->data, "Hg", 2) == 0) {
        static char *res = "OK";
        return send_packet(ctx, res, strlen(res));
    }

    if (strncmp(packet->data, "Hc", 2) == 0) {
        static char *res = "OK";
        return send_packet(ctx, res, strlen(res));
    }

    if (strcmp(packet->data, "Hc-1") == 0) {
        static char *res = "OK";
        return send_packet(ctx, res, strlen(res));
    }

    if (packet->data[0] == 'm') {
        char *split = nullptr;

        const u32 addr = strtol(&packet->data[1], &split, 16);
        const size_t len = strtol(split + 1, nullptr, 16);

        if (addr + len >= CPU_MEMORY_SIZE) {
            static char *res = "E14";
            return send_packet(ctx, res, strlen(res));
        }

        const size_t data_size = 2 * len;
        char *data = malloc((data_size * sizeof(*data)) + 1);
        memset(data, 0, data_size * sizeof(*data));

        for (size_t i = 0; i < len; ++i) {
            const u8 byte = ctx->cpu.memory[addr + i];
            sprintf(data + (2 * i), "%02X", byte);
        }

        const bool result = send_packet(ctx, data, data_size);

        free(data);
        data = nullptr;

        return result;
    }

    if (packet->data[0] == 'g') {
        static constexpr size_t REGS_DATA_SIZE = 8L * (CPU_REGS_SIZE + 1L);

        char buf[REGS_DATA_SIZE + 1] = {};
        size_t pos = 0;

        for (size_t i = 0; i < CPU_REGS_SIZE; ++i)
            append_register_hex(buf, &pos, ctx->cpu.registers[i]);

        append_register_hex(buf, &pos, ctx->cpu.pc);

        return send_packet(ctx, buf, REGS_DATA_SIZE);
    }

    return send_packet(ctx, "", 0);
}

int main(int argc, const char *argv[])
{
    int port = 0;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_INTEGER('p', "port", &port, "port to listen for gdb on", nullptr, 0, 0),
        OPT_BOOLEAN('v', "verbose", &verbose, nullptr, nullptr, 0, 0),
        OPT_END(),
    };

    struct argparse argparse;

    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, "\nA quick and dirty RISC-V 32 CPU emulator written in C.",
                      nullptr);

    argc = argparse_parse(&argparse, argc, argv);

    if (argc < 1) {
        argparse_usage(&argparse);
        return EXIT_FAILURE;
    }

    if (port == 0)
        port = DEFAULT_PORT;

    const char *const filename = argv[0];
    printf("Opening %s\n", filename);

    FILE *const file = fopen(filename, "r");

    if (file == nullptr) {
        perror("Could not open file");
        return EXIT_FAILURE;
    }

    Cpu cpu = Cpu_new();
    size_t fread_result = 0;

    if (!Cpu_load_file(&cpu, file, &fread_result)) {
        fprintf(stderr, "Program is too large\n");
        return EXIT_FAILURE;
    }

    if (!fread_result) {
        perror("Could not read file");
        return EXIT_FAILURE;
    }

    fclose(file);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Could not create socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr = {.s_addr = INADDR_ANY},
        .sin_port = htons(port),
    };

    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, nullptr, sizeof(int));

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
        perror("Could not bind server");
        return EXIT_FAILURE;
    }

    atexit(cleanup);

    struct sigaction action = {};
    action.sa_handler = &sigint_handler;
    sigaction(SIGINT, &action, &old_action);

    if (listen(server_sock, 10) < 0) {
        perror("Could not listen");
        return EXIT_FAILURE;
    }

    printf("Server listening on port %i\n", port);

    struct sockaddr_in client_addr;
    socklen_t addr_len = 0;
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);

    Context ctx = {
        .cpu = cpu,
        .client_sock = client_sock,
        .no_ack_mode = false,
        .running = false,
    };

    while (true) {
        GdbPacket packet = {};

        if (ctx.running) {
            Cpu_step(&ctx.cpu);

            char ch = '\0';
            if (!try_read_buf(ctx.client_sock, &ch) || ch != 0x03)
                continue;

            static const char *const RES = "S02";
            send_packet(&ctx, RES, strlen(RES));
            ctx.running = false;
        }

        if (!receive_packet(client_sock, &packet))
            break;

        if (!ctx.no_ack_mode) {
            const u8 computed_checksum = data_checksum(packet.data, packet.data_size);

            if (computed_checksum != packet.checksum) {
                GdbPacket_destroy(&packet);
                write(client_sock, "-", 1); // NACK
                continue;
            }
        }

        ver_printf("=======================================\n");
        ver_printf("Packet: '%s'\n", packet.data);

        if (!ctx.no_ack_mode)
            write(client_sock, "+", 1); // ACK

        handle_packet(&ctx, &packet);

        GdbPacket_destroy(&packet);
    }

    close(client_sock);

    Cpu_destroy(&cpu);

    return EXIT_SUCCESS;
}
