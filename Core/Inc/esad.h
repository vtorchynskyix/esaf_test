#ifndef ESAD_H_INCLUDED
#define ESAD_H_INCLUDED

enum esad_code {
    version = 0,
    safe = 1,
    arm = 2,
    no_radio = 4,
    fire = 5,
    fault = 0xFF
};

const static uint32_t default_data = 0xFEDC;
const static uint32_t sadcp_version = 0x0001;

typedef struct {
    union {
        uint32_t mem;
        uint8_t buf[4];
        struct {
            unsigned command : 8;
            unsigned data : 16;
            unsigned seq : 4;
            unsigned chksum : 4;
        };
    };
} esad_msg_t;

typedef struct {
    uint16_t uptime_secs;
    uint16_t secs_after_arm;
    uint8_t fc_version_ok;
    uint8_t arm_switch_on, fire_switch_on;
    uint8_t fired_latch;
    uint8_t fc_has_noradio;
    uint16_t fc_timeout;
    uint8_t comm_timeout;
} state_t;

uint8_t esad_msg_checksum(const esad_msg_t* msg);
void set_esad_msg_checksum(esad_msg_t* msg);

#endif
