#include "cpu.h"
#include "protocol.h"
#include "stdinc.h"
#include "str.h"
#include "util.h"
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

static constexpr u16 DEFAULT_PORT = 3333;

static bool verbose = false;

static GdbServer server = {};
static int client_sock = -1;

static void cleanup(void)
{
    printf("Shutting down gracefully...\n");

    if (client_sock >= 0)
        close(client_sock);

    close(server.sock);
}

static struct sigaction old_action;

static void sigint_handler([[maybe_unused]] const int sig_no)
{
    cleanup();
    sigaction(SIGINT, &old_action, nullptr);
    kill(0, SIGINT);
}

typedef struct Context {
    Cpu cpu;
} Context;

[[nodiscard]] static String handle_q_packet(const Packet *const packet, GdbServer *const server)
{
    if (strncmp(packet->data.data, "qSupported", strlen("qSupported")) == 0)
        return String_from("QStartNoAckMode+");

    if (strcmp(packet->data.data, "QStartNoAckMode") == 0) {
        GdbServer_set_no_ack_mode(server, true);
        return String_from("OK");
    }

    if (strcmp(packet->data.data, "qfThreadInfo") == 0)
        return String_from("m1");

    if (strcmp(packet->data.data, "qsThreadInfo") == 0)
        return String_from("l");

    if (strcmp(packet->data.data, "qC") == 0)
        return String_from("QC1");

    if (strcmp(packet->data.data, "qTStatus") == 0)
        return String_new();

    return String_new();
}

[[nodiscard]] static String handle_v_packet(const Packet *const packet)
{
    if (strcmp(packet->data.data, "vCont?") == 0) {
        return String_from("vCont;c;s;t");
    }

    return String_new();
}

static void String_push_register_hex(String *const s, u32 value)
{
    for (size_t i = 0; i < 4; ++i) {
        const u8 byte = (value >> (8 * i)) & 0xFF;
        String_push_hex(s, byte);
    }
}

[[nodiscard]] String packet_handler(void *const ctx_raw, const Packet *const packet,
                                    GdbServer *const server, BufSock *const client)
{
    Context *const ctx = ctx_raw;

    if (packet->data.size == 0)
        return String_new();

    if (packet->data.data[0] == 'q' || packet->data.data[0] == 'Q')
        return handle_q_packet(packet, server);

    if (packet->data.data[0] == 'v')
        return handle_v_packet(packet);

    if (packet->data.data[0] == '?')
        return String_from("S05");

    if (packet->data.data[0] == 's') {
        Cpu_step(&ctx->cpu);
        return String_from("S05");
    }

    if (packet->data.data[0] == 'c') {
        char ch = '\0';

        while (!BufSock_try_read_buf(client, &ch) || ch != 0x03)
            Cpu_step(&ctx->cpu);

        return String_from("S02");
    }

    if (strncmp(packet->data.data, "Hg", 2) == 0)
        return String_from("OK");

    if (strncmp(packet->data.data, "Hc", 2) == 0)
        return String_from("OK");

    if (strcmp(packet->data.data, "Hc-1") == 0)
        return String_from("OK");

    if (packet->data.data[0] == 'm') {
        char *split = nullptr;

        const u32 addr = strtol(&packet->data.data[1], &split, 16);
        const size_t len = strtol(split + 1, nullptr, 16);

        if (addr + len >= CPU_MEMORY_SIZE)
            return String_from("E14");

        String s = String_with_capacity(2 * len);

        for (size_t i = 0; i < len; ++i) {
            const u8 byte = ctx->cpu.memory[addr + i];
            String_push_hex(&s, byte);
        }

        return s;
    }

    if (packet->data.data[0] == 'g') {
        String s = String_with_capacity(8L * (CPU_REGS_SIZE + 1L));

        for (size_t i = 0; i < CPU_REGS_SIZE; ++i)
            String_push_register_hex(&s, ctx->cpu.registers[i]);

        String_push_register_hex(&s, ctx->cpu.pc);
        return s;
    }

    return String_new();
}

int main(int argc, const char *argv[])
{
    int port = DEFAULT_PORT;

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
    printf("Reading %s\n", filename);

    size_t data_size = 0;
    u8 *data = load_file(filename, &data_size);

    if (data == nullptr) {
        perror("Could not read file");
        return EXIT_FAILURE;
    }

    Cpu cpu = Cpu_new();
    Cpu_load_data(&cpu, 0x0, data, data_size);

    free(data);
    data = nullptr;

    Context ctx = {
        .cpu = cpu,
    };

    if (!GdbServer_new(packet_handler, &ctx, &server)) {
        perror("Could not create server");
        return EXIT_FAILURE;
    }

    if (!GdbServer_listen(&server, port)) {
        perror("Could not bind server to port");
        return EXIT_FAILURE;
    }

    atexit(cleanup);

    const struct sigaction action = {.sa_handler = &sigint_handler};
    sigaction(SIGINT, &action, &old_action);

    printf("Server listening on port %i\n", port);

    struct sockaddr_in client_addr = {};
    socklen_t addr_len = 0;
    client_sock = GdbServer_accept_connection(&server, &client_addr, &addr_len);

    if (client_sock < 0) {
        perror("Could not accept connection");
        return EXIT_FAILURE;
    }

    printf("Connection from %s\n", inet_ntoa(client_addr.sin_addr));

    if (!GdbServer_run(&server, client_sock)) {
        fprintf(stderr, "Something went wrong.\n");
        return EXIT_FAILURE;
    }

    Cpu_destroy(&cpu);

    return EXIT_SUCCESS;
}
