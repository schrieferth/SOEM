#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "soem/soem.h"

#define IO_MAP_SIZE 4096
#define CYCLE_US 10000

static ecx_contextt ctx;
static char iomap[IO_MAP_SIZE];
static int expected_wkc = 0;
static int output_bytes = 0;
static int input_bytes = 0;

static void cycle_once(void) {
    ecx_send_processdata(&ctx);
    ecx_receive_processdata(&ctx, EC_TIMEOUTRET);
    osal_usleep(CYCLE_US);
}

static void cycle_many(int count) {
    for (int i = 0; i < count; i++) {
        cycle_once();
    }
}

static uint8_t *outputs_base(void) {
    return (uint8_t *)ctx.grouplist[0].outputs;
}

static uint8_t *inputs_base(void) {
    return (uint8_t *)ctx.grouplist[0].inputs;
}

static void safe_off(void) {
    uint8_t *out = outputs_base();
    if (out && output_bytes > 0) {
        memset(out, 0, (size_t)output_bytes);
        cycle_many(10);
    }
}

static void shutdown_bus(void) {
    safe_off();
    ctx.slavelist[0].state = EC_STATE_INIT;
    ecx_writestate(&ctx, 0);
    ecx_close(&ctx);
}

static bool bringup(const char *ifname) {
    memset(&ctx, 0, sizeof(ctx));
    memset(iomap, 0, sizeof(iomap));
    if (!ecx_init(&ctx, (char *)ifname)) {
        fprintf(stdout, "{\"ok\":false,\"error\":\"init_failed\",\"interface\":\"%s\"}\n", ifname);
        fflush(stdout);
        return false;
    }
    ecx_config_init(&ctx);
    if (ctx.slavecount <= 0) {
        fprintf(stdout, "{\"ok\":false,\"error\":\"no_slaves\",\"interface\":\"%s\"}\n", ifname);
        fflush(stdout);
        ecx_close(&ctx);
        return false;
    }
    ec_groupt *group = &ctx.grouplist[0];
    ecx_config_map_group(&ctx, iomap, 0);
    ecx_configdc(&ctx);
    output_bytes = group->Obytes;
    input_bytes = group->Ibytes;
    expected_wkc = (group->outputsWKC * 2) + group->inputsWKC;
    cycle_many(20);
    ctx.slavelist[0].state = EC_STATE_OPERATIONAL;
    ecx_writestate(&ctx, 0);
    ecx_statecheck(&ctx, 0, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE);
    if (ctx.slavelist[0].state != EC_STATE_OPERATIONAL) {
        fprintf(stdout, "{\"ok\":false,\"error\":\"operational_failed\",\"state\":%d}\n", ctx.slavelist[0].state);
        fflush(stdout);
        shutdown_bus();
        return false;
    }
    cycle_many(20);
    fprintf(stdout, "{\"ok\":true,\"state\":\"running\",\"slaves\":%d,\"obytes\":%d,\"ibytes\":%d,\"expected_wkc\":%d}\n", ctx.slavecount, output_bytes, input_bytes, expected_wkc);
    fflush(stdout);
    return true;
}

static void handle_status(void) {
    fprintf(stdout, "{\"ok\":true,\"state\":\"running\",\"slaves\":%d,\"obytes\":%d,\"ibytes\":%d,\"expected_wkc\":%d}\n", ctx.slavecount, output_bytes, input_bytes, expected_wkc);
    fflush(stdout);
}

static void handle_read(char *path, int byte_offset, int bit_offset) {
    cycle_many(3);
    uint8_t *in = inputs_base();
    if (!in || byte_offset < 0 || byte_offset >= input_bytes || bit_offset < 0 || bit_offset > 7) {
        fprintf(stdout, "{\"ok\":false,\"error\":\"read_offset_out_of_range\",\"path\":\"%s\",\"byte\":%d,\"bit\":%d}\n", path, byte_offset, bit_offset);
        fflush(stdout);
        return;
    }
    int byte_value = in[byte_offset];
    bool value = (byte_value & (1 << bit_offset)) != 0;
    fprintf(stdout, "{\"ok\":true,\"path\":\"%s\",\"value\":%s,\"input_byte\":%d,\"byte\":%d,\"bit\":%d}\n", path, value ? "true" : "false", byte_value, byte_offset, bit_offset);
    fflush(stdout);
}

static void handle_write(char *path, int byte_offset, int bit_offset, int value) {
    uint8_t *out = outputs_base();
    if (!out || byte_offset < 0 || byte_offset >= output_bytes || bit_offset < 0 || bit_offset > 7) {
        fprintf(stdout, "{\"ok\":false,\"error\":\"write_offset_out_of_range\",\"path\":\"%s\",\"byte\":%d,\"bit\":%d}\n", path, byte_offset, bit_offset);
        fflush(stdout);
        return;
    }
    if (value) {
        out[byte_offset] |= (uint8_t)(1 << bit_offset);
    } else {
        out[byte_offset] &= (uint8_t)~(1 << bit_offset);
    }
    cycle_many(20);
    fprintf(stdout, "{\"ok\":true,\"path\":\"%s\",\"value\":%s,\"output_byte\":%d,\"byte\":%d,\"bit\":%d}\n", path, value ? "true" : "false", out[byte_offset], byte_offset, bit_offset);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stdout, "{\"ok\":false,\"error\":\"usage\",\"message\":\"usage: soem_pdo_server <iface>\"}\n");
        return 2;
    }
    if (!bringup(argv[1])) {
        return 1;
    }
    char line[1024];
    while (fgets(line, sizeof(line), stdin)) {
        char cmd[32] = {0};
        char path[512] = {0};
        int byte_offset = 0;
        int bit_offset = 0;
        int value = 0;
        if (sscanf(line, "%31s", cmd) != 1) {
            continue;
        }
        if (strcmp(cmd, "STATUS") == 0) {
            handle_status();
        } else if (strcmp(cmd, "READ") == 0 && sscanf(line, "%31s %511s %d %d", cmd, path, &byte_offset, &bit_offset) == 4) {
            handle_read(path, byte_offset, bit_offset);
        } else if (strcmp(cmd, "WRITE") == 0 && sscanf(line, "%31s %511s %d %d %d", cmd, path, &byte_offset, &bit_offset, &value) == 5) {
            handle_write(path, byte_offset, bit_offset, value);
        } else if (strcmp(cmd, "SAFE_OFF") == 0) {
            safe_off();
            fprintf(stdout, "{\"ok\":true,\"safe_off\":true}\n");
            fflush(stdout);
        } else if (strcmp(cmd, "STOP") == 0) {
            safe_off();
            fprintf(stdout, "{\"ok\":true,\"stopped\":true}\n");
            fflush(stdout);
            break;
        } else {
            fprintf(stdout, "{\"ok\":false,\"error\":\"unknown_command\",\"raw\":\"%s\"}\n", cmd);
            fflush(stdout);
        }
    }
    shutdown_bus();
    return 0;
}
