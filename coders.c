/*
 * coders.c
 *
 *
 *  Created on: 17.11.2024
 *  Author: Eemeli Kyröläinen / University of Oulu
 *
 *  Morse code encoder and decoder.
 *
 */


#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "message.h"
#include "coders.h"

# define TABLE_LEN 36
# define MAX_SYMBOL_LEN 5

const char ALPHABET[TABLE_LEN] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',
                         'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R',
                         'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '1',
                         '2', '3', '4', '5', '6', '7', '8', '9', '0'};

const char *MORSE_TABLE[TABLE_LEN] = {".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..",
                             ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.",
                             "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--..", ".----",
                             "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----.", "-----"};

static uint8_t get_conversion_index(char *chr, uint8_t _idx) {
    /*
     * _idx is the index of '\0' character.
     * Use _idx 0 for conversions from alphabet to morse
     */
    uint8_t i = 0;
    if (_idx == 0) {
        if ((64 < *chr && *chr < 91) || (96 < *chr && *chr < 123)) {
            for (;i < TABLE_LEN - 10; i++) {
                if (toupper(*chr) == ALPHABET[i]) {
                    return i;
                }
            }
        } else {
            i = TABLE_LEN - 10;
            for (;i < TABLE_LEN; i++) {
                if (*chr == ALPHABET[i]) {
                    return i;
                }
            }
        }
    } else {
        for (;i < TABLE_LEN; i++) {
            if (MORSE_TABLE[i][_idx] == '\0') {
                uint8_t j = 0;
                char *tmp = chr;
                while (*tmp != '\0') {
                    if (MORSE_TABLE[i][j] != *tmp) {
                        break;
                    } else if (j + 1 == _idx) {
                        return i;
                    }
                    j++;
                    tmp++;
                }
            }
        }
    }
    return TABLE_LEN;
}

void encode(char *chr, msg* message, uint16_t len) {
    /*
     * Encoder from latin alphabet (including numbers 0-9) to morse code
     * @param char *chr input string
     * @param msg *message output destination as msg data struct
     * @param uint16_t len is length of input string
     */
    uint8_t i = 0;
    uint8_t j = 0;
    uint16_t k = 0;
    while(*chr != '\0' && k < len) {
        if (*chr == ' ') {
            msgAppend(message, ' ');
        } else {
            i = get_conversion_index(chr, 0);
            if (i < TABLE_LEN) {
                const char *code_char = MORSE_TABLE[i];
                while (*code_char != '\0') {
                    msgAppend(message, *code_char);
                    code_char++;
                }
            } else {
                // All unrecognized characters are encoded as '?'.
                msgAppend(message, '?');
            }
            msgAppend(message, ' ');
        }
        chr++;
        k++;
    }
    j = 0;
    for (; j < 2; j++) {
        msgAppend(message, ' ');
    }
}


void decode(char *chr, msg *message, uint16_t len) {
    /*
     * Decoder from morse code to latin alphabet (including numbers 0-9)
     * @param char *chr input string
     * @param msg *message output destination as msg data struct
     * @param uint16_t len is length of input string
     */
    char tmp[MAX_SYMBOL_LEN + 1];
    uint8_t i = 0;
    uint8_t j = 0;
    uint8_t space_count = 0;
    uint16_t k = 0;

    while (*chr != '\0' && k < len) {
        if (*chr == ' ') {
            space_count++;
            if (space_count == 1 && i > 0) {
                tmp[i] = '\0';
                j = get_conversion_index(tmp, i);
                if (j < TABLE_LEN) {
                    msgAppend(message, ALPHABET[j]);
                } else {
                    msgAppend(message, '?');
                }
                memset(tmp, '\0', i);
                i = 0;
            } if (space_count == 3) {
                break;
            }
        } else {
            if (space_count == 2) {
                msgAppend(message, ' ');
            }
            space_count = 0;
            tmp[i] = *chr;
            i++;
        }
        chr++;
        k++;
    }
}
