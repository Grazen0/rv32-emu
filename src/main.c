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
    printf("Reading %s\n", filename);

    size_t elf_data_size = 0;
    u8 *elf_data = load_file(filename, &elf_data_size);

    if (elf_data == nullptr) {
        perror("Could not read file");
        return EXIT_FAILURE;
    }

    const Elf32_Ehdr *ehdr = {};
    const Elf32_Phdr *phdrs = {};

    const ElfResult elf_result = parse_elf(elf_data, elf_data_size, &ehdr, &phdrs);
    if (elf_result != ElfResult_Ok) {
        fprintf(stderr, "Could not load ELF: %s\n", ElfResult_display(elf_result));
        return EXIT_FAILURE;
    }

    printf("Loading program...\n");

    Cpu cpu = Cpu_new();
    cpu.pc = ehdr->e_entry;

    for (size_t i = 0; i < ehdr->e_phnum; ++i) {
        ver_printf("Header %zu ==============================\n", i + 1);
        const Elf32_Phdr *const phdr = &phdrs[i];

        ver_printf("type:    0x%02X\n", phdr->p_type);
        ver_printf("flags:   0b%03B\n", phdr->p_flags);
        ver_printf("offset:  0x%02X\n", phdr->p_offset);
        ver_printf("vaddr:   0x%02X\n", phdr->p_vaddr);
        ver_printf("paddr:   0x%02X\n", phdr->p_paddr);
        ver_printf("filesz:  0x%02X\n", phdr->p_filesz);
        ver_printf("memsz:   0x%02X\n", phdr->p_memsz);
        ver_printf("align:   0x%02X\n", phdr->p_align);
        ver_printf("align:   0x%02X\n", phdr->p_align);
        ver_printf("\n");

        if (phdr->p_filesz != 0) {
            for (size_t i = 0; i < phdr->p_filesz; ++i) {
                ver_printf("%02X ", elf_data[phdr->p_offset + i]);

                if ((i % 16) == 15 || i == phdr->p_filesz - 1)
                    ver_printf("\n");
            }

            ver_printf("\n");
        }

        if (phdr->p_type == PT_LOAD)
            memcpy(&cpu.memory[phdr->p_vaddr], &elf_data[phdr->p_offset], phdr->p_filesz);
    }

    free(elf_data);
    elf_data = nullptr;

    Context ctx = {
        .cpu = cpu,
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

    Cpu_destroy(&cpu);

    return EXIT_SUCCESS;
}
