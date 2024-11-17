/*
 * message.h
 *
 *
 *  Created on: 17.11.2024
 *  Author: Eemeli Kyröläinen / University of Oulu
 *
 *  Dynamic message array for SensorTag morse code project.
 *
 */

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <xdc/runtime/System.h>

#define DEFAULT_MSG_LEN 100
#define MSG_MAX_SIZE 60000

typedef struct msg {
    uint16_t count;
    uint16_t size;
    char *data;
} msg;

void msgInit(msg *message);
void msgRealloc(msg *message);
void msgDestroy(msg *message);
void msgClear(msg *message);
void msgAppend(msg *message, const char chr);

#endif /* MESSAGE_H_ */
