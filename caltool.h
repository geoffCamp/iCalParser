/*********************
caltool.h - Prototypes and  structures for caltool.c
Last Modified: 3:30pm Feb-22-16
Geofferson Camp (gcamp@mail.uoguelph.ca)
0658817
********/

#ifndef CALTOOL_H
#define CALTOOL_H A2_RevA

#define _GNU_SOURCE     // for getdate_r
#define _XOPEN_SOURCE   // for strptime
#include <time.h>
#include <stdio.h>
#include "calutil.h"

/* Symbols used to send options to command execution modules */

typedef enum {
    OEVENT,     // events
    OPROP,      // properties
    OTODO,      // to-do items
    ALL,        // all items
} CalOpt;

typedef enum {
    INFO = 0,
    EXTRACT,
    FILTER,
    COMBINE,
    NONE,
} ComType;

typedef struct InfoDetails {
    int events;
    int todos;
    int others;
    int subComps;
    int props;
    char ** organizers;
    int orgSize;
    struct tm ** toStruct;
    struct tm ** fromStruct;
    time_t from;
    time_t to;
} InfoDetails;

typedef struct ExtractEvent {
    time_t time;
    struct tm ** timeStruct;
    char * summary;
} ExtractEvent;

/* iCalendar tool functions */

CalStatus calInfo( const CalComp *comp, int lines, FILE *const txtfile );
CalStatus calExtract( const CalComp *comp, CalOpt kind, FILE *const txtfile );
CalStatus calFilter( const CalComp *comp, CalOpt content, time_t datefrom, time_t dateto, FILE *const icsfile );
CalStatus calCombine( const CalComp *comp1, const CalComp *comp2, FILE *const icsfile );

#endif
