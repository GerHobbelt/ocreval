/**********************************************************************
 *
 *  accrpt.c
 *
 *  Author: Stephen V. Rice
 *  Author: Eddie Antonio Santos
 *
 * Copyright 2017 Eddie Antonio Santos
 *
 * Copyright 1996 The Board of Regents of the Nevada System of Higher
 * Education, on behalf, of the University of Nevada, Las Vegas,
 * Information Science Research Institute
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You
 * may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 *
 **********************************************************************/

#include <utf8proc.h>

#include "accrpt.h"
#include "sort.h"
#include "ocreval_version.h"

#include <assert.h>
#include <string.h>

#define TITLE    "ocreval Accuracy Report Version " OCREVAL_VERSION "\n"
#define DIVIDER  "-----------------------------------\n"

#define CLASS_OFFSET  29
#define CONF_OFFSET   20
#define LINE_LENGTH   100

static char line[LINE_LENGTH];

/**********************************************************************/

static void update_class(Accclass *class, long count, long missed)
{
    class->count  += count;
    class->missed += missed;
}
/**********************************************************************/

void add_class(Accdata *accdata, Charvalue value;, long count, long missed)
{
    update_class(&accdata->large_class[charclass(value)], count, missed);
    update_class(&accdata->total_class, count, missed);
    update_class(&accdata->small_class[value], count, missed);
}
/**********************************************************************/

void add_conf(Accdata *accdata, char *key, long errors, long marked)
{
    Conf *conf;
    conf = table_lookup(&accdata->conftable, key);
    if (conf)
    {
        conf->errors += errors;
        conf->marked += marked;
    }
    else
    {
        conf = NEW(Conf);
        conf->key = strdup(key);
        conf->errors = errors;
        conf->marked = marked;
        table_insert(&accdata->conftable, conf);
    }
}
/**********************************************************************/

static Boolean read_line(FILE *f)
{
    return(fgets(line, sizeof(line) - 1, f) ? True : False);
}
/**********************************************************************/

static Boolean read_value(FILE *f, long *value, long *sum_value)
{
    if (read_line(f) && sscanf(line, "%ld", value) == 1)
    {
        *sum_value += *value;
        return(True);
    }
    else
        return(False);
}
/**********************************************************************/

static Boolean read_ops(FILE *f, Accops *sum_ops)
{
    Accops ops;
    if (read_line(f) && sscanf(line, "%ld %ld %ld %ld", &ops.ins, &ops.subst,
    &ops.del, &ops.errors) == 4)
    {
        sum_ops->ins    += ops.ins;
        sum_ops->subst  += ops.subst;
        sum_ops->del    += ops.del;
        sum_ops->errors += ops.errors;
        return(True);
    }
    else
        return(False);
}
/**********************************************************************/

static Boolean read_two(FILE *f, long *value1, long *value2)
{
    return(read_line(f) && sscanf(line, "%ld %ld", value1, value2) == 2 ?
    True : False);
}
/**********************************************************************/

static Charvalue read_char(char *line)
{
    Charvalue value = INVALID_CHARVALUE;
    char *next_byte = line;

    /* Check for start delimiter at byte 0. */
    if (*next_byte != '{') {
        return INVALID_CHARVALUE;
    }
    next_byte++;

    /* Check for first distinguishing byte at byte 1 */
    if (*next_byte == '<') {
        /* Peek at the following byte. */
        if (next_byte[1] == '}') {
            /* It's a stray '<' */
            value = '<';
        } else {
            /* Peek at following byte, yet again! */
            if (next_byte[1] == '\\' && next_byte[2] == 'n') {
                value = NEWLINE;
                next_byte += 3; /* consume three bytes: <\n */
            } else {
                /* Decode the hexadecimal escape. */
                next_byte++;
                int offset;
                int ret = sscanf(next_byte, "%x%n", &value, &offset);
                if (ret != 1) {
                    return INVALID_CHARVALUE;
                }
                next_byte += offset;
            }

            if (*next_byte != '>') {
                return INVALID_CHARVALUE;
            }
        }

        next_byte++;
    } else {
        /* It's a UTF-8 character. */
        /* Read at most 4 characters, and decode the utf-8 bytes. */
        int bytes_read = utf8proc_iterate((utf8proc_uint8_t *) next_byte,
                                          4, (utf8proc_int32_t *) &value);
        if (bytes_read < 1) {
            return INVALID_CHARVALUE;
        }
        next_byte += bytes_read;

        if (value == COMBINING_MARK_BASE && *next_byte != '}') {
            /* This is the base of a combining character. The *actual*
             * character is the combining mark that follows. */
            /* Decode the next UTF-8 bytes. It should always be non-ASCII. */
            int bytes_read = utf8proc_iterate((utf8proc_uint8_t *) next_byte,
                                              4, (utf8proc_int32_t *) &value);
            /* It should be non-ASCII, so there should be AT LEAST two bytes
             * to read. */
            if (bytes_read < 2) {
                return INVALID_CHARVALUE;
            }
            next_byte += bytes_read;
        }
    }

    /* Check for end delimiter. */
    if (*next_byte == '}') {
        return value;
    }

    return INVALID_CHARVALUE;
}
/**********************************************************************/

void read_accrpt(Accdata *accdata, char *filename)
{
    FILE *f;
    long characters, errors, value1, value2;
    Charvalue value3;
    f = open_file(filename, "r");
    if (read_line(f) && strncmp(line, TITLE, sizeof(TITLE) - 3) == 0 &&
    read_line(f) && strcmp(line, DIVIDER) == 0 &&
    read_value(f, &characters, &accdata->characters) &&
    read_value(f, &errors, &accdata->errors) &&
    read_line(f) && read_line(f) &&
    read_value(f, &value1, &accdata->reject_characters) &&
    read_value(f, &value1, &accdata->suspect_markers) &&
    read_value(f, &value1, &accdata->false_marks) &&
    read_line(f) && read_line(f) && read_line(f) && read_line(f) &&
    read_ops(f, &accdata->marked_ops) &&
    read_ops(f, &accdata->unmarked_ops) &&
    read_ops(f, &accdata->total_ops) && read_line(f))
    {
        while (read_line(f) && line[0] != NEWLINE);
        if (errors > 0 && read_line(f))
            while (read_two(f, &value1, &value2)) {
                /* TODO: bug here: does not handle UTF-8 or <bracket>
                 * <escaped> or {<} properly... */
                add_conf(accdata, &line[CONF_OFFSET], value1, value2);
            }
        if (characters > 0 && read_line(f))
            while (read_two(f, &value1, &value2)) {
                value3 = read_char(line + CLASS_OFFSET);
                if (value3 == INVALID_CHARVALUE) {
                    error_string("invalid character in", (filename ? filename : "stdin"));
                }
                add_class(accdata, value3, value1, value2);
            }
    }
    else
        error_string("invalid format in", (filename ? filename : "stdin"));
    close_file(f);
}
/**********************************************************************/

static void write_value(FILE *f, long value, char *string)
{
    fprintf(f, "%8ld   %s\n", value, string);
}
/**********************************************************************/

static void write_pct(FILE *f, long numerator, long denominator)
{
    if (denominator == 0)
        fprintf(f, "  ------");
    else
        fprintf(f, "%8.2f", 100.0 * numerator / denominator);
}
/**********************************************************************/

static void write_ops(FILE *f, Accops *ops, char *string)
{
    fprintf(f, "%8ld %8ld %8ld %8ld   %s\n", ops->ins, ops->subst, ops->del,
    ops->errors, string);
}
/**********************************************************************/

static void write_class(FILE *f, Accclass *class, char *string, Charvalue value)
{
    char buffer[STRING_SIZE];
    fprintf(f, "%8ld %8ld ", class->count, class->missed);
    write_pct(f, class->count - class->missed, class->count);
    if (string)
        fprintf(f, "   %s\n", string);
    else
    {
        char_to_string(False, value, buffer, True);
        fprintf(f, "   {%s}\n", buffer);
    }
}
/**********************************************************************/

static void write_conf(FILE *f, Conf *conf)
{
    fprintf(f, "%8ld %8ld   %s", conf->errors, conf->marked, conf->key);
}
/**********************************************************************/

static int compare_conf(Conf *conf1, Conf *conf2)
{
    if (conf1->errors != conf2->errors)
        return(conf2->errors - conf1->errors);
    if (conf1->marked != conf2->marked)
        return(conf2->marked - conf1->marked);
    return(ustrcmp(conf1->key, conf2->key));
}
/**********************************************************************/

void write_accrpt(Accdata *accdata, char *filename)
{
    FILE *f;
    long i;
    f = open_file(filename, "w");
    fprintf(f, "%s%s", TITLE, DIVIDER);
    write_value(f, accdata->characters, "Characters");
    write_value(f, accdata->errors, "Errors");
    write_pct(f, accdata->characters - accdata->errors, accdata->characters);
    fprintf(f, "%%  Accuracy\n\n");
    write_value(f, accdata->reject_characters, "Reject Characters");
    write_value(f, accdata->suspect_markers, "Suspect Markers");
    write_value(f, accdata->false_marks, "False Marks");
    write_pct(f, accdata->reject_characters + accdata->suspect_markers,
    accdata->characters);
    fprintf(f, "%%  Characters Marked\n");
    write_pct(f, accdata->characters - accdata->unmarked_ops.errors,
    accdata->characters);
    fprintf(f, "%%  Accuracy After Correction\n");
    fprintf(f, "\n     Ins    Subst      Del   Errors\n");
    write_ops(f, &accdata->marked_ops, "Marked");
    write_ops(f, &accdata->unmarked_ops, "Unmarked");
    write_ops(f, &accdata->total_ops, "Total");
    fprintf(f, "\n   Count   Missed   %%Right\n");
    for (i = 0; i < MAX_CHARCLASSES; i++)
        if (accdata->large_class[i].count > 0)
            write_class(f, &accdata->large_class[i], charclass_name(i), 0);
    write_class(f, &accdata->total_class, "Total", 0);
    if (accdata->errors > 0)
    {
        table_in_array(&accdata->conftable);
        sort(accdata->conftable.count, accdata->conftable.array, compare_conf);
        fprintf(f, "\n  Errors   Marked   Correct-Generated\n");
        for (i = 0; i < accdata->conftable.count; i++)
            write_conf(f, accdata->conftable.array[i]);
    }
    if (accdata->characters > 0)
    {
        fprintf(f, "\n   Count   Missed   %%Right\n");
        for (i = 0; i < NUM_CHARVALUES; i++)
            if (accdata->small_class[i].count > 0)
                write_class(f, &accdata->small_class[i], NULL, i);
    }
    close_file(f);
}
