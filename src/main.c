#include "cpu.h"
#include "elf.h"
#include "elf_helpers.h"
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

static GdbServer server = {};
static int client_sock = -1;

static void cleanup(void)
{
    printf("Shutting down gracefully...\n");

    if (client_sock >= 0)
        close(client_sock);

    GdbServer_destroy(&server);
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
    String stop_signal;
} Context;

static void Context_destroy(Context *const ctx)
{
    Cpu_destroy(&ctx->cpu);
    String_destroy(&ctx->stop_signal);
}

static bool Context_set_stop_signal(Context *const ctx, const char *const stop_signal)
{
    if (strcmp(ctx->stop_signal.data, stop_signal) == 0)
        return false;

    String_destroy(&ctx->stop_signal);
    ctx->stop_signal = String_from(stop_signal);

    return true;
}

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

[[nodiscard]] static String handle_read_mem(Context *const ctx, const Packet *const packet)
{
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

[[nodiscard]] static String handle_write_mem(Context *const ctx, const Packet *const packet)
{
    char *split_1 = nullptr;
    char *split_2 = nullptr;

    const u32 addr = strtol(&packet->data.data[1], &split_1, 16);
    const size_t len = strtol(split_1 + 1, &split_2, 16);

    const char *const byte_data = split_2 + 1;

    if (2 * len != strlen(byte_data))
        return String_from("E01"); // Bad packet

    if (addr + len >= CPU_MEMORY_SIZE)
        return String_from("E14"); // Bad address

    for (size_t i = 0; i < len; ++i) {
        char buf[3] = {};
        memcpy(buf, byte_data + (2 * i), 2);

        const u8 byte = strtol(buf, nullptr, 16);
        ctx->cpu.memory[addr + i] = byte;
    }

    return String_from("OK");
}

[[nodiscard]] String handle_read_regs(Context *const ctx)
{
    String s = String_with_capacity(8L * (CPU_REGS_SIZE + 1L));

    for (size_t i = 0; i < CPU_REGS_SIZE; ++i)
        String_push_register_hex(&s, ctx->cpu.registers[i]);

    String_push_register_hex(&s, ctx->cpu.pc);
    return s;
}

[[nodiscard]] u32 u32_read_hex_le(const char *const str)
{
    u32 out = 0x0;

    for (size_t j = 0; j < 4; ++j) {
        char buf[3] = {};
        memcpy(buf, &str[2 * j], 2);

        const u8 byte = strtol(buf, nullptr, 16);
        out |= (u32)byte << (8 * j);
    }

    return out;
}

[[nodiscard]] String handle_write_regs(Context *const ctx, const Packet *const packet)
{
    if (packet->data.size != 1 + 8L * (CPU_REGS_SIZE + 1))
        return String_from("E01"); // Bad packet

    size_t pos = 1;

    for (size_t i = 0; i < CPU_REGS_SIZE; ++i) {
        ctx->cpu.registers[i] = u32_read_hex_le(&packet->data.data[pos]);
        pos += 8L;
    }

    ctx->cpu.pc = u32_read_hex_le(&packet->data.data[pos]);
    return String_from("OK");
}

[[nodiscard]] static String handle_continue(Context *const ctx, BufSock *const client)
{
    char ch = '\0';

    while (!BufSock_try_read_buf(client, &ch) || ch != 0x03) {
        const CpuStepResult result = Cpu_step(&ctx->cpu);

        if (result == CpuStepResult_Break) {
            Context_set_stop_signal(ctx, "S05");
            return String_clone(ctx->stop_signal);
        }

        if (result == CpuStepResult_IllegalInstruction) {
            Context_set_stop_signal(ctx, "S04");
            return String_clone(ctx->stop_signal);
        }
    }

    Context_set_stop_signal(ctx, "S02");
    return String_clone(ctx->stop_signal);
}

[[nodiscard]] static String packet_handler(void *const ctx_raw, const Packet *const packet,
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
        return String_clone(ctx->stop_signal);

    if (packet->data.data[0] == 's') {
        const CpuStepResult result = Cpu_step(&ctx->cpu);

        if (result == CpuStepResult_IllegalInstruction)
            Context_set_stop_signal(ctx, "S04");
        else
            Context_set_stop_signal(ctx, "S05");

        return String_clone(ctx->stop_signal);
    }

    if (packet->data.data[0] == 'c')
        return handle_continue(ctx, client);

    if (strncmp(packet->data.data, "Hg", 2) == 0)
        return String_from("OK");

    if (strncmp(packet->data.data, "Hc", 2) == 0)
        return String_from("OK");

    if (strcmp(packet->data.data, "Hc-1") == 0)
        return String_from("OK");

    if (packet->data.data[0] == 'm')
        return handle_read_mem(ctx, packet);

    if (packet->data.data[0] == 'M')
        return handle_write_mem(ctx, packet);

    if (packet->data.data[0] == 'g')
        return handle_read_regs(ctx);

    if (packet->data.data[0] == 'G')
        return handle_write_regs(ctx, packet);

    return String_new();
}

static bool load_elf_to_cpu(const char *const filename, Cpu *const cpu)
{
    printf("Reading %s\n", filename);

    size_t elf_data_size = 0;
    u8 *elf_data = load_file(filename, &elf_data_size);

    if (elf_data == nullptr) {
        perror("Could not read file");
        return false;
    }

    const Elf32_Ehdr *ehdr = nullptr;
    const Elf32_Phdr *phdrs = nullptr;

    const ElfResult elf_result = parse_elf(elf_data, elf_data_size, &ehdr, &phdrs);

    if (elf_result != ElfResult_Ok) {
        fprintf(stderr, "Could not load ELF: %s\n", ElfResult_display(elf_result));
        return false;
    }

    printf("Loading program...\n");
    ver_printf("\n");

    cpu->pc = ehdr->e_entry;

    for (size_t i = 0; i < ehdr->e_phnum; ++i) {
        const Elf32_Phdr *const phdr = &phdrs[i];
        print_phdr_debug(phdrs, i, elf_data, elf_data_size);

        if (phdr->p_type == PT_LOAD) {
            const ElfResult result =
                load_phdr(cpu->memory, CPU_MEMORY_SIZE, phdr, i, elf_data, elf_data_size);

            if (result != ElfResult_Ok) {
                fprintf(stderr, "Could not load an ELF program header: %s\n",
                        ElfResult_display(result));
                return EXIT_FAILURE;
            }
        }
    }

    free(elf_data);
    return true;
}

int main(int argc, const char *argv[])
{
    int port = DEFAULT_PORT;
    bool verbose = false;

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
    set_verbose(verbose);

    if (argc < 1) {
        argparse_usage(&argparse);
        return EXIT_FAILURE;
    }

    const char *const filename = argv[0];

    Cpu cpu = Cpu_new();

    if (!load_elf_to_cpu(filename, &cpu))
        return EXIT_FAILURE;

    Context ctx = {
        .cpu = cpu,
        .stop_signal = String_from("S05"),
    };

    if (!GdbServer_new(packet_handler, &ctx, &server)) {
        perror("Could not create server");
        return EXIT_FAILURE;
    }

    if (!GdbServer_listen(&server, port)) {
        perror("Could not start server");
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

    const GdbResult result = GdbServer_handle_client(&server, client_sock);

    if (result != GdbResult_Ok) {
        fprintf(stderr, "Error: %s\n", GdbResult_display(result));
        return EXIT_FAILURE;
    }

    Context_destroy(&ctx);

    return EXIT_SUCCESS;
}
