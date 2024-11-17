/*
 * message.c
 *
 *
 *  Created on: 17.11.2024
 *  Author: Eemeli Kyröläinen / University of Oulu
 *
 *  Dynamic message array for SensorTag morse code project.
 *
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <xdc/runtime/System.h>

# define DEFAULT_MSG_LEN 100
# define MSG_MAX_SIZE 60000

typedef struct msg {
    uint16_t count;
    uint16_t size;
    char *data;
} msg;

void msgInit(msg *message) {
    message->count = 0;
    message->size = DEFAULT_MSG_LEN;
    message->data = malloc(message->size * sizeof(char));
    if (message->data == NULL) {
        System_abort("Error: Message initialization failed!\n");
    }
    memset(message->data, '\0', message->size);
}

void msgRealloc(msg *message) {
    if (message->size + DEFAULT_MSG_LEN > MSG_MAX_SIZE) {
        System_abort("Error: message maximum size exeeced!\n");
    }
    message->size += DEFAULT_MSG_LEN;
    char *tmp = realloc(message->data, message->size * sizeof(char));
    if (tmp == NULL || message->size > MSG_MAX_SIZE) {
        free(message->data);
        message->data = NULL;
        System_abort("Error: Ran out of memory for resizing message!\n");
    }
    message->data = tmp;
}

void msgDestroy(msg *message) {
    free(message->data);
    message->data = NULL;
}

void msgClear(msg *message) {
    message->count = 0;
    memset(message->data, '\0', message->size);
}

void msgAppend(msg *message, const char chr) {
    if (chr != '\0') {
        message->data[message->count] = chr;
        message->count++;
    }
    if (message->count >= message->size) {
        msgRealloc(message);
    }
    message->data[message->count] = '\0';
}
