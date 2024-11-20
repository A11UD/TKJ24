/*
 * coders.h
 *
 *
 *  Created on: 17.11.2024
 *  Author: Eemeli Kyröläinen / University of Oulu
 *
 *  Morse code encoder and decoder.
 *
 */

#ifndef CODERS_H_
#define CODERS_H_

#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "message.h"

# define TABLE_LEN 36
# define MAX_SYMBOL_LEN 5

extern const char ALPHABET[TABLE_LEN];
extern const char *MORSE_TABLE[TABLE_LEN];

void encode(char *chr, msg* message, uint16_t len);
void decode(char *chr, msg *message, uint16_t len);

#endif /* CODERS_H_ */
