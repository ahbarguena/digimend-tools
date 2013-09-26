/*
 * Copyright (C) 2013 Nikolai Kondrashov
 *
 * This file is part of huion-tools.
 *
 * Huion-tools is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Huion-tools is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with huion-tools; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * @author Nikolai Kondrashov <spbnick@gmail.com>
 */

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <strings.h>

#define GENERIC_ERROR(_fmt, _args...) \
    fprintf(stderr, _fmt "\n", ##_args)

#define GENERIC_FAILURE(_fmt, _args...) \
    GENERIC_ERROR("Failed to " _fmt, ##_args)

#define ERROR_CLEANUP(_fmt, _args...) \
    do {                                \
        GENERIC_ERROR(_fmt, ##_args);   \
        goto cleanup;                   \
    } while (0)

#define FAILURE_CLEANUP(_fmt, _args...) \
    do {                                \
        GENERIC_FAILURE(_fmt, ##_args); \
        goto cleanup;                   \
    } while (0)

struct decoder {
    uint8_t     id;
    int       (*decode)(const uint8_t *ptr, int len);
};

static int
decode_params(const uint8_t *ptr, int len)
{
    const int   length              = 10;
    const int   max_x_off           = 0;
    const int   max_y_off           = 2;
    const int   max_pressure_off    = 6;
    const int   resolution_off      = 8;
    int         start;

    /* Find the start of the parameters block by first finding its end */
    for (start = len - 1; start >= 0 && ptr[start] == 0; start--);
    if (ptr[start] == 0x08 && start >= length)
        start -= length;
    else
        start = 0;

#define FIELD(_label, _offset) \
    do {                                                                \
        printf("%14s: ", _label);                                       \
        if ((start + _offset) < len - 1) {                              \
            printf("%u\n",                                              \
                   ptr[start + _offset] |                               \
                    ((unsigned int)ptr[start + (_offset) + 1] << 8));   \
        } else {                                                        \
            printf("N/A\n");                                            \
        }                                                               \
    } while (0)
    FIELD("Max X", max_x_off);
    FIELD("Max Y", max_y_off);
    FIELD("Max pressure", max_pressure_off);
    FIELD("Resolution", resolution_off);
#undef FIELD
    fflush(stdout);
    return 0;
}

/* List of string descriptor decoders */
static const struct decoder desc_list[] = {
    {0x64, decode_params},
    {0x00, NULL}
};

static int
decode_desc(const uint8_t *buf, int len)
{
    const struct decoder   *d;

    if (len == 0) {
        GENERIC_ERROR("String descriptor without index");
        return 1;
    }

    for (d = desc_list; d->decode != NULL; d++) {
        if (d->id == *buf)
            return d->decode(buf + 1, len - 1);
    }

    return 0;
}

/* List of chunk decoders */
static const struct decoder chunk_list[] = {
    {'S',   decode_desc},
    {'\0',  NULL}
};

static int
decode_chunk(const uint8_t *buf, int len)
{
    const struct decoder   *d;

    assert(len > 0);

    for (d = chunk_list; d->decode != NULL; d++) {
        if (d->id == *buf)
            return d->decode(buf + 1, len - 1);
    }

    GENERIC_ERROR("Uknown chunk type '%c'", *buf);
    return 1;
}

static int
decode(void)
{
    int             result      = 1;
    char           *word        = NULL;
    char           *end;
    uint8_t         buf[256];
    uint8_t        *p           = buf;
    unsigned long   n;

    assert(sizeof(buf) > 0);

    while (scanf("%ms", &word) == 1) {
        /* If it's a single character chunk type */
        if (word[0] != '\0' && !isxdigit(word[0]) && word[1] == '\0') {
            if (p != buf && decode_chunk(buf, p - buf) != 0)
                FAILURE_CLEANUP("decode chunk");
            p = buf;
            n = word[0];
        } else {
            if (p == buf)
                ERROR_CLEANUP("Expecting chunk type indicator");
            errno = 0;
            n = strtoul(word, &end, 16);
            if (*end != '\0' || errno != 0 || n > UINT8_MAX)
                ERROR_CLEANUP("Invalid byte \"%s\"", word);
            if (p >= buf + sizeof(buf))
                ERROR_CLEANUP("Descriptor too long");
        }
        *p++ = n;
        free(word);
        word = NULL;
    };

    if (p != buf && decode_chunk(buf, p - buf) != 0)
        FAILURE_CLEANUP("decode chunk");

    result = 0;

cleanup:
    free(word);
    return result;
}

static void
usage(FILE *file, const char *name)
{
    fprintf(file,
            "Usage: %s\n"
            "Decode a Huion tablet probe dump.\n"
            "\n",
            name);
}

int
main(int argc, char **argv)
{
    const char *name;

    name = rindex(argv[0], '/');
    if (name == NULL)
        name = argv[0];
    else
        name++;

    if (argc != 1) {
        fprintf(stderr, "Invalid number of arguments\n");
        usage(stderr, name);
        exit(1);
    }

    setbuf(stdout, NULL);
    return decode();
}