#include "cpu.h"
#include "gdb.h"
#include "stdinc.h"
#include "str.h"
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

static const char *const usages[] = {
    "rv32-emu [options] [--] <filename>",
    nullptr,
};

static constexpr u16 DEFAULT_PORT = 1234;

static bool verbose = false;

static int server_sock = 0;
static int client_sock = 0;

void ver_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

void ver_printf(const char *const fmt, ...)
{
    if (verbose) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    }
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

static constexpr size_t BUF_CAPACITY = 128;
static char packet_buf[BUF_CAPACITY];
static size_t packet_buf_size = 0;
static size_t packet_buf_head = 0;

void refill_buf(const int sock)
{
    packet_buf_size = read(sock, packet_buf, BUF_CAPACITY * sizeof(char));
    packet_buf_head = 0;
}

bool read_buf(const int sock, char *const dest)
{
    if (packet_buf_head >= packet_buf_size) {
        refill_buf(sock);

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

bool receive_packet(const int sock, Packet *const dest)
{
    String data = String_new();
    char ch = '\0';

    while (ch != '$') {
        if (!read_buf(sock, &ch))
            return false;
    }

    while (true) {
        if (!read_buf(sock, &ch))
            return false;

        if (ch == '#')
            break;

        String_push(&data, ch);
    }

    char checksum_str[3] = {};

    if (!read_buf(sock, &checksum_str[0]))
        return false;

    if (!read_buf(sock, &checksum_str[1]))
        return false;

    *dest = (Packet){
        .data = data,
        .checksum = strtol(checksum_str, nullptr, 16),
    };
    return true;
}

typedef struct Context {
    Cpu cpu;
    int client_sock;
    bool no_ack_mode;
    bool running;
} Context;

static inline void String_push_hex(String *const str, const u8 byte)
{
    char buf[3] = {};
    sprintf(buf, "%02x", byte);
    String_push_raw(str, buf);
}

/**
 * Takes ownership of data.
 */
bool send_response(Context *const ctx, String data)
{
    ver_printf("Response: %s\n", data.data);

    const u8 checksum = data_checksum(&data);

    String raw_data = String_with_capacity(data.size + 4);
    String_push(&raw_data, '$');
    String_push_str(&raw_data, data);
    String_push(&raw_data, '#');
    String_push_hex(&raw_data, checksum);

    write(ctx->client_sock, raw_data.data, raw_data.size);

    if (!ctx->no_ack_mode) {
        while (true) {
            char ch = '\0';

            if (!read_buf(ctx->client_sock, &ch))
                return false;

            if (ch == '+')
                break;

            if (ch == '-')
                write(ctx->client_sock, raw_data.data, raw_data.size);
        }
    }

    String_destroy(&raw_data);
    String_destroy(&data);
    return true;
}

bool handle_q_packet(Context *const ctx, const Packet *const packet)
{
    if (strncmp(packet->data.data, "qSupported", strlen("qSupported")) == 0)
        return send_response(ctx, String_from("QStartNoAckMode+"));

    if (strcmp(packet->data.data, "QStartNoAckMode") == 0) {
        ctx->no_ack_mode = true;
        return send_response(ctx, String_from("OK"));
    }

    if (strcmp(packet->data.data, "qfThreadInfo") == 0)
        return send_response(ctx, String_from("m1"));

    if (strcmp(packet->data.data, "qsThreadInfo") == 0)
        return send_response(ctx, String_from("l"));

    if (strcmp(packet->data.data, "qC") == 0)
        return send_response(ctx, String_from("QC1"));

    if (strcmp(packet->data.data, "qTStatus") == 0)
        return send_response(ctx, String_new());

    return send_response(ctx, String_new());
}

bool handle_v_packet(Context *const ctx, const Packet *const packet)
{
    if (strcmp(packet->data.data, "vCont?") == 0) {
        return send_response(ctx, String_from("vCont;c;s;t"));
    }

    return send_response(ctx, String_new());
}

static void String_push_register_hex(String *const s, u32 value)
{
    for (size_t i = 0; i < 4; ++i) {
        const u8 byte = (value >> (8 * i)) & 0xFF;
        String_push_hex(s, byte);
    }
}

bool handle_packet(Context *const ctx, const Packet *const packet)
{
    if (packet->data.size == 0)
        return false;

    if (packet->data.data[0] == 'q' || packet->data.data[0] == 'Q')
        return handle_q_packet(ctx, packet);

    if (packet->data.data[0] == 'v')
        return handle_v_packet(ctx, packet);

    if (packet->data.data[0] == '?')
        return send_response(ctx, String_from("S05"));

    if (packet->data.data[0] == 's') {
        Cpu_step(&ctx->cpu);
        return send_response(ctx, String_from("S05"));
    }

    if (packet->data.data[0] == 'c') {
        ctx->running = true;
        return true;
    }

    if (strncmp(packet->data.data, "Hg", 2) == 0)
        return send_response(ctx, String_from("OK"));

    if (strncmp(packet->data.data, "Hc", 2) == 0)
        return send_response(ctx, String_from("OK"));

    if (strcmp(packet->data.data, "Hc-1") == 0)
        return send_response(ctx, String_from("OK"));

    if (packet->data.data[0] == 'm') {
        char *split = nullptr;

        const u32 addr = strtol(&packet->data.data[1], &split, 16);
        const size_t len = strtol(split + 1, nullptr, 16);

        if (addr + len >= CPU_MEMORY_SIZE)
            return send_response(ctx, String_from("E14"));

        String s = String_with_capacity(2 * len);

        for (size_t i = 0; i < len; ++i) {
            const u8 byte = ctx->cpu.memory[addr + i];
            String_push_hex(&s, byte);
        }

        const bool result = send_response(ctx, s);

        return result;
    }

    if (packet->data.data[0] == 'g') {
        String s = String_with_capacity(8L * (CPU_REGS_SIZE + 1L));

        for (size_t i = 0; i < CPU_REGS_SIZE; ++i)
            String_push_register_hex(&s, ctx->cpu.registers[i]);

        String_push_register_hex(&s, ctx->cpu.pc);

        return send_response(ctx, s);
    }

    return send_response(ctx, String_new());
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
        if (ctx.running) {
            Cpu_step(&ctx.cpu);

            char ch = '\0';
            if (!try_read_buf(ctx.client_sock, &ch) || ch != 0x03)
                continue;

            send_response(&ctx, String_from("S02"));
            ctx.running = false;
        }

        Packet packet = {};

        if (!receive_packet(client_sock, &packet))
            break;

        if (!ctx.no_ack_mode) {
            const u8 computed_checksum = data_checksum(&packet.data);

            if (computed_checksum != packet.checksum) {
                Packet_destroy(&packet);
                write(client_sock, "-", 1); // NACK
                continue;
            }
        }

        ver_printf("=======================================\n");
        ver_printf("Packet: '%s'\n", packet.data.data);

        if (!ctx.no_ack_mode)
            write(client_sock, "+", 1); // ACK

        handle_packet(&ctx, &packet);

        Packet_destroy(&packet);
    }

    close(client_sock);

    Cpu_destroy(&cpu);

    return EXIT_SUCCESS;
}
