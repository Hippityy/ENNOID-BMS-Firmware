#ifndef MOD_COMMAND_POLICY_H
#define MOD_COMMAND_POLICY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Independent of hardware, checked before any payload access or command action. */
bool modCommandPolicyAllows(uint8_t command, const uint8_t *payload,
                            size_t length, bool commissioning);
#endif
