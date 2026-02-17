#include <stdint.h>
#include "esad.h"

uint8_t esad_msg_checksum(const esad_msg_t* msg) {
  return ((msg->mem & 0xF)
          + (msg->mem >> 4 & 0xF)
          + (msg->mem >> 8 & 0xF)
          + (msg->mem >> 12 & 0xF)
          + (msg->mem >> 16 & 0xF)
          + (msg->mem >> 20 & 0xF)
          + (msg->mem >> 24 & 0xF)
          + (msg->mem >> 28 & 0xF)) & 0xF;
}

void set_esad_msg_checksum(esad_msg_t* msg) {
  msg->chksum += ~esad_msg_checksum(msg) + 1u;
}
