#include "soem/soem.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    ecx_contextt context;
    uint8 map[4096];
} Fieldbus;

static int roundtrip(Fieldbus *fb) {
    ecx_send_processdata(&fb->context);
    return ecx_receive_processdata(&fb->context, EC_TIMEOUTRET);
}

static int run(const char *iface, int output_bit, int input_bit, int value) {
    Fieldbus fb;
    ecx_contextt *ctx;
    ec_groupt *grp;
    ec_slavet *slave0;
    int slave_count;
    int expected_wkc = 0;
    int wkc = 0;
    int i;
    memset(&fb, 0, sizeof(fb));
    ctx = &fb.context;
    grp = ctx->grouplist;
    if (!ecx_init(ctx, iface)) {
        printf("ERROR init_failed iface=%s\n", iface);
        return 2;
    }
    slave_count = ecx_config_init(ctx);
    if (slave_count <= 0) {
        printf("ERROR no_slaves\n");
        ecx_close(ctx);
        return 3;
    }
    ecx_config_map_group(ctx, fb.map, 0);
    ecx_configdc(ctx);
    ecx_statecheck(ctx, 0, EC_STATE_SAFE_OP, EC_TIMEOUTSTATE * 4);
    roundtrip(&fb);
    slave0 = ctx->slavelist;
    slave0->state = EC_STATE_OPERATIONAL;
    ecx_writestate(ctx, 0);
    for (i = 0; i < 40; ++i) {
        roundtrip(&fb);
        ecx_statecheck(ctx, 0, EC_STATE_OPERATIONAL, EC_TIMEOUTSTATE / 20);
        if (slave0->state == EC_STATE_OPERATIONAL) break;
        osal_usleep(10000);
    }
    if (slave0->state != EC_STATE_OPERATIONAL) {
        printf("ERROR not_operational state=0x%04x slaves=%d obytes=%d ibytes=%d\n", slave0->state, slave_count, grp->Obytes, grp->Ibytes);
        ecx_readstate(ctx);
        for (i = 1; i <= ctx->slavecount; ++i) {
            ec_slavet *slave = ctx->slavelist + i;
            printf("SLAVE slot=%d name=%s state=0x%04x al=0x%04x\n", i, slave->name, slave->state, slave->ALstatuscode);
        }
        ecx_close(ctx);
        return 4;
    }
    expected_wkc = (grp->outputsWKC * 2) + grp->inputsWKC;
    if (grp->Obytes < 1 || grp->Ibytes < 1) {
        printf("ERROR no_process_bytes obytes=%d ibytes=%d\n", grp->Obytes, grp->Ibytes);
        slave0->state = EC_STATE_INIT;
        ecx_writestate(ctx, 0);
        ecx_close(ctx);
        return 5;
    }
    if (value) {
        grp->outputs[0] |= (uint8)(1u << output_bit);
    } else {
        grp->outputs[0] &= (uint8)~(1u << output_bit);
    }
    for (i = 0; i < 50; ++i) {
        wkc = roundtrip(&fb);
        osal_usleep(5000);
    }
    int input_value = (grp->inputs[0] >> input_bit) & 1u;
    printf("RESULT slaves=%d expected_wkc=%d wkc=%d obytes=%d ibytes=%d output_byte0=0x%02x input_byte0=0x%02x output_bit=%d output_value=%d input_bit=%d input_value=%d\n",
           slave_count, expected_wkc, wkc, grp->Obytes, grp->Ibytes, grp->outputs[0], grp->inputs[0], output_bit, value, input_bit, input_value);
    grp->outputs[0] &= (uint8)~(1u << output_bit);
    for (i = 0; i < 20; ++i) {
        roundtrip(&fb);
        osal_usleep(5000);
    }
    slave0->state = EC_STATE_INIT;
    ecx_writestate(ctx, 0);
    ecx_close(ctx);
    return input_value == value ? 0 : 10;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        printf("Usage: %s IFACE OUTPUT_BIT INPUT_BIT VALUE\n", argv[0]);
        return 1;
    }
    return run(argv[1], atoi(argv[2]), atoi(argv[3]), atoi(argv[4]) ? 1 : 0);
}
