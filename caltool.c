/**************************
caltool.c - Information parsing and manipulation functions for iCalendar
Last Modified: 3:30pm Feb-22-16
Geofferson Camp (gcamp@mail.uoguelph.ca)
0658817
****************************/

#include "caltool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calutil.h"
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_DATESTRING 20
#define MAX_FILENAME 1000
#define MAX_ORG 1000
#define MAX_ORGNAME 300
#define MAX_EVENTS 5000
#define MAX_XPROPS 5000
#define MAX_SUMMARY 2000
#define MAX_XNAME 100
#define MATCH_STRING 10
#define ARG_KEY_LENGTH 5
#define DATE_TO 1
#define DATE_FROM 0
#define NOKIND 7

/*
Parse input to determine command
INPUT: user arguments
OUTPUT: Command to initiate
*/
ComType modSelect(char ** input);

/*
Parse type of extract/filter
INPUT: user submitted arguments
OUTPUT: CalOpt indicating type of extract/filter
*/
CalOpt getKind(char * kindStr);

/*
Get the position of a date string
INPUT: command line args, date you are looking for
OUTPUT: position of date string, returns 0 when not found
*/
int getDateKey (char ** input, int argCount, char key[ARG_KEY_LENGTH]);

/*
Get the current time
INPUT: to/from to decide on hours and minutes
OUTPUT: current seconds since epoche
*/
time_t getNowTime (int toFrom);

/*
Parse user submitted time range
INPUT: To or from designation
OUTPUT: time specified in seconds since epoche
*/
time_t getToFromTime (int toFrom, char ** argv, int argc);

/*
Prints information on fatal main errors
INPUT: CalStatus'
OUTPUT: NA
*/
void printError (CalStatus tool, CalStatus util);

int main (int argc, char ** argv) {
    ComType handle; 
    CalStatus utilStatus = {.code = OK, .lineto = 0, .linefrom = 0};
    CalStatus toolStatus = {.code = OK, .lineto = 0, .linefrom = 0};
    bool overHeadOk = true;
    CalComp * stdComp;
    CalComp * combMore;
    CalOpt kind = NOKIND;
    time_t datefrom = 0, dateto = 0;
    FILE * combineFile; 

    if (argc < 2) {
        fprintf(stderr, "invalid command. caltool option required.\n");
        return EXIT_FAILURE;
    }

    utilStatus = readCalFile(stdin,&stdComp); 

    if (utilStatus.code != OK) {
        fprintf(stderr,"read calendar failed with code:%d line %d\n",utilStatus.code, utilStatus.lineto);
        return EXIT_FAILURE;
    }
    handle = modSelect(argv);
    switch (handle) {
        case INFO:
            if (argc < 2 || argc > 2) {
                fprintf(stderr,"Invalid input. Correct usage eg: caltool -info < events.ics\n");
                overHeadOk = false;
            } else {
                toolStatus = calInfo(stdComp,utilStatus.lineto,stdout);
            }
            break;
        case EXTRACT:
            if (argc < 3 || argc > 3) {
                fprintf(stderr,"Invalid input. Correct usage eg: caltool -extract e < events.ics\n");
                overHeadOk = false;
            } else {
                kind = getKind(argv[2]);
                if (kind == OEVENT || kind == OPROP) {
                    toolStatus = calExtract(stdComp,kind,stdout);
                } else {
                   fprintf(stderr,"Invalid input argument. second arg must be 'x' or 'e'\n");
                   overHeadOk = false;
                }
            }
            break;
        case FILTER:
            if (argc < 3 || argc == 6) {
                fprintf(stderr,"Invalid input. Correct usage eg: caltool -filter t < events.ics\n");
                overHeadOk = false;
            }
            datefrom = getToFromTime(DATE_FROM,argv,argc); 
            dateto = getToFromTime(DATE_TO,argv,argc); 
            if (datefrom == -1 || dateto == -1) {
                overHeadOk = false;
            }
            if (dateto != 0 && datefrom != 0 && dateto < datefrom && dateto != -1 && datefrom != -1) {
                fprintf(stderr,"date error, 'to' before 'from'\n"); 
                overHeadOk = false;   
            }
            if (overHeadOk == true) {
                kind = getKind(argv[2]);
            } else {
                kind = NOKIND;
            }
            if ((kind == OEVENT || kind == OTODO) && overHeadOk == true) {
                toolStatus = calFilter(stdComp, kind, datefrom, dateto, stdout);
            } else {
                if (overHeadOk != false) {
                    fprintf(stderr,"Invalid argument. Second arg must be 't' or 'e' \n"); 
                    overHeadOk = false;    
                }
            }
            break;
        case COMBINE:
             if (argc < 3) {
                fprintf(stderr,
                  "Invalid input. Correct usage eg: caltool -combine events2.ics < events.ics\n");
                overHeadOk = false;
            } else {
                if ((combineFile = fopen(argv[2],"r")) != NULL) {
                    utilStatus = readCalFile(combineFile,&combMore);
                    if (utilStatus.code == OK) {
                        toolStatus = calCombine(stdComp,combMore,stdout);
                        freeCalComp(combMore);
                        fclose(combineFile);
                    } else {
                        fprintf(stderr,"second iCalendar file could not be read\n");
                    }  
                } else {
                    fprintf(stderr,"second iCalendar file could not be opened\n");
                    overHeadOk = false;
                }
            }
            break;
        default:
            fprintf(stderr,
              "Invalid input. Must use -info, -extract, -filter, or -combine as first arg\n");
            overHeadOk = false;
            break;
    } 
    if (utilStatus.code == OK) {
        freeCalComp(stdComp);
    }
    if (toolStatus.code == OK && utilStatus.code == OK && overHeadOk == true) { 
        return EXIT_SUCCESS;
    } else {
        printError(toolStatus,utilStatus);
        return EXIT_FAILURE;
    }
}

void printError (CalStatus tool, CalStatus util) {
    if (tool.code != OK) {
        if (tool.code == IOERR) {
            fprintf(stderr,"IOERR at line: %d\n",tool.lineto);
        } else if (tool.code == NOCAL) {
            fprintf(stderr,"error: NOCAL\n");
        } 
    }
    if (util.code != OK) {
        fprintf(stderr,"calUtil error code: %d at line %d\n",util.code,util.lineto);
    }
}

time_t getToFromTime(int toFrom, char ** argv, int argc) {
    int dateKey = 0;
    int dateErr = 0;
    struct tm dateStruct = {0};
    char dateString[MAX_DATESTRING];
    time_t toReturn = 0;

    if (toFrom == DATE_FROM) {
        dateKey = getDateKey(argv,argc,"from");
    } else {
        dateKey = getDateKey(argv,argc,"to");    
    }
    if (dateKey == 0) {
        toReturn = 0;
    } else if (dateKey == -1) {
        toReturn = getNowTime(toFrom);    
    } else {
        strncpy(dateString,argv[dateKey],MAX_DATESTRING);
        if ((dateErr = getdate_r(dateString,&dateStruct)) != 0) {
            assert(dateErr != 6);
            if (dateErr == 1 || dateErr == 2 || dateErr == 3 || dateErr == 4 || dateErr == 5) {
                fprintf(stderr,"Problem with DATEMSK environment variable or template file\n");
            } else if (dateErr == 7 || dateErr == 8) {
                fprintf(stderr,"Date \"%s\" could not be interpreted\n",argv[dateKey]);
            }
            toReturn = -1;
        } else {
            if (toFrom == DATE_TO) {
                dateStruct.tm_hour = 23;
                dateStruct.tm_min = 59;
                dateStruct.tm_sec = 0;
            } else {
                dateStruct.tm_hour = 0;
                dateStruct.tm_min = 0;
                dateStruct.tm_sec = 0;
            }
            dateStruct.tm_isdst = -1;
            toReturn = mktime((&dateStruct)); 
        }
    }
    return toReturn;
}

time_t getNowTime (int toFrom) {
    struct tm * dateStruct = {0};
    time_t rawTime = 0;
    
    time(&rawTime);
    dateStruct = localtime(&rawTime);  

    if (toFrom == 1) {
        dateStruct->tm_hour = 23;
        dateStruct->tm_min = 59;
        dateStruct->tm_sec = 0;
    } else {
        dateStruct->tm_hour = 0;
        dateStruct->tm_min = 0;
        dateStruct->tm_sec = 0;
    }
    dateStruct->tm_isdst = -1;
    return mktime(dateStruct);
}

int getDateKey (char ** input, int argCount, char key[ARG_KEY_LENGTH]) {
    for (int i = 0; i<argCount; i++) {
        if (strcmp(input[i],key) == 0) {
            if (strcmp(input[i+1],"today") == 0) {
                return -1;
            } else {
                return i + 1;
            }
        }
    }
    return 0;
}

CalOpt getKind(char * kindStr) {
    if (strcmp(kindStr,"e") == 0) {
        return OEVENT;
    } else if (strcmp(kindStr,"x") == 0) {
        return OPROP;
    } else if (strcmp(kindStr,"t") == 0) { 
        return OTODO;
    } else {
        return NOKIND;
    }
}

ComType modSelect(char ** input) {
    ComType toReturn;

    if (strcmp(input[1],"-info") == 0) {
        toReturn = INFO;  
    } else if (strcmp(input[1],"-extract") == 0) {
        toReturn = EXTRACT;
    } else if (strcmp(input[1],"-filter") == 0) {
        toReturn = FILTER;
    } else if (strcmp(input[1],"-combine") == 0) {
        toReturn = COMBINE;
    } else {
        toReturn = NONE;
    }
    return toReturn;
}

/*
Make a copy of a CalComponent
INPUT:calComp to copy, filter restrictions
OUTPUT: address of comp copy
*/
CalComp * makeCopy (const CalComp * comp, CalOpt content, time_t from, time_t to, ComType caller);

/*
Make a copy or a calProp
INPUT: calProp to copy
OUTPUT: address of prop copy
*/
CalProp * copyProp (CalProp * prop);

/*
Copy contents of parameter
INPUT: parameter to copy
OUTPUT: address of param copy
*/
CalParam * copyParam (CalParam * param);

/*
Add a parameter to a property
INPUT: calParam and calProp 
OUTPUT: NA
*/
void addParam (CalProp * prop, CalParam * param);

/*
Add top level props from comp2 to comp1
INPUT: Two CalComps
OUTPUT: NA
*/
void copyProps (CalComp * copy1, CalComp * copy2);

CalStatus calCombine (const CalComp * comp1, const CalComp * comp2, FILE * const icsfile) {
    CalComp * comp1copy;
    CalComp * comp2copy;
    CalStatus toReturn = {.code =0, .linefrom = 0, .lineto = 0};

    comp1copy = makeCopy(comp1,ALL,0,0,COMBINE);
    comp2copy = makeCopy(comp2,ALL,0,0,COMBINE);

    //add copy2 props to copy1 discluding prodid and version
    copyProps(comp1copy,comp2copy);    

    //add copy2 comps to copy1 comps[] will need to realloc
    comp1copy = realloc(comp1copy,
      sizeof(CalComp)+sizeof(CalComp*)*(comp1copy->ncomps+comp2copy->ncomps));
    assert(comp1copy != NULL);
    for (int i = 0; i<comp2copy->ncomps; i++) {
        comp1copy->comp[comp1copy->ncomps] = comp2copy->comp[i];
        comp1copy->ncomps = comp1copy->ncomps + 1;
    }
    
    //write copy1
    toReturn = writeCalComp(icsfile,comp1copy); 

    //free copies
    freeCalComp(comp1copy);
    free(comp2copy->name);
    free(comp2copy);

    return toReturn;
}

void copyProps(CalComp * copy1, CalComp * copy2) {
    CalProp * propHolder = NULL;
    CalProp * copy1End = NULL;
    CalProp * copy2Prod = NULL;
    CalProp * copy2Ver = NULL;

    propHolder = copy1->prop;
    if (propHolder == NULL) {
        copy1End = NULL;
    } else {
        while (propHolder->next != NULL) {
            propHolder = propHolder->next;    
        }
        copy1End = propHolder;
    } 
    propHolder = copy2->prop;
    while (propHolder != NULL) {
        if (strcmp(propHolder->name,"VERSION") != 0 && strcmp(propHolder->name,"PRODID") != 0) {
            copy1End->next = propHolder;
            copy1End = propHolder;
            copy1->nprops++;
        } else {
            if (strcmp(propHolder->name,"VERSION") == 0) {
                copy2Ver = propHolder;
            } else {
                copy2Prod = propHolder;
            }
        }
        propHolder = propHolder->next;
    }

    //free properties not copied
    free(copy2Prod->name);
    free(copy2Ver->name);
    free(copy2Prod->value);
    free(copy2Ver->value);
    free(copy2Ver);
    free(copy2Prod);    
}

/*
Check if comp contains a parameter within the date range
INPUT: comp to check and date range
OUTPUT: if in range indicator
*/
int checkDate (CalComp * comp, time_t from, time_t to, ComType caller);

/*
Searches property for a date and makes not of it accordingly
INPUT: CalProp to search for time value
OUTPUT: int indicating if prop has a time/date, returns 0 if no date 
*/
time_t findDate (CalProp * prop, struct tm ** timeStruct, ComType caller);

CalStatus calFilter(const CalComp * comp, CalOpt content, time_t datefrom, time_t dateto, 
  FILE * const icsfile) {
    CalStatus toReturn = {.code = OK, .lineto = 0, .linefrom = 0};
    CalComp * copiedComp;
       
    copiedComp = makeCopy(comp,content,datefrom,dateto,FILTER);

    if (copiedComp->ncomps == 0) {
        toReturn.code = NOCAL;
    } else {
        toReturn = writeCalComp(icsfile,copiedComp);
    }

    freeCalComp(copiedComp);
    return toReturn;
}

CalComp * makeCopy (const CalComp * comp, CalOpt content, time_t from, time_t to, ComType caller) {
    CalComp * compCopy;
    CalProp * propHolder;
    CalProp  * propCopy;
    CalComp * compToAdd;
    char toMatch[MATCH_STRING];

    if (content == OEVENT) {
        strncpy(toMatch,"VEVENT",MATCH_STRING);
    } else {
        strncpy(toMatch,"VTODO",MATCH_STRING);
    }
    propHolder = comp->prop;
    compCopy = malloc(sizeof(CalComp) + sizeof(CalComp*)*comp->ncomps);
    assert(compCopy != NULL);
    compCopy->name = malloc(sizeof(char)*strlen(comp->name)+1);
    assert(compCopy->name != NULL);
    strncpy(compCopy->name,comp->name,strlen(comp->name)+1);
    compCopy->prop = NULL;
    while (propHolder != NULL) { 
        propCopy = copyProp(propHolder);
        addProp(compCopy,propCopy);       
        propHolder = propHolder->next;
        propCopy = NULL;
    }
    compCopy->ncomps = 0;
    for (int i =0; i<comp->ncomps; i++) {
        compToAdd = NULL;
        if (strcmp(toMatch,comp->comp[i]->name) == 0 || content == ALL) {
            compToAdd = makeCopy(comp->comp[i],ALL,from,to,caller);
            if (checkDate(compToAdd,from,to,caller) == 1) {
                compCopy->comp[compCopy->ncomps] = compToAdd;
                compCopy->ncomps = compCopy->ncomps + 1;
            } else {
                freeCalComp(compToAdd);
            }
       }  
    }
    return compCopy;
}

int checkDate (CalComp * comp, time_t from, time_t to, ComType caller) {
    CalProp * holder;
    time_t time = 0;
    struct tm * timeStruct;
    int toReturn = 0;

    holder = comp->prop;

    //if dates arent set, dont filter by date
    if (to == 0 && from == 0) {
        return 1;
    }
    while (holder != NULL) {
        time = findDate(holder,&timeStruct,caller);
        if (time == 0) {
            holder = holder->next;
            continue;
        }
        if (from != 0 && to != 0) {
            if (time <= to && time >= from) {
                return 1;
            }
        } else if (from != 0 && to == 0) {
            if (time >= from) {
                return 1;
            }
        } else if (to != 0 && from == 0) {
            if (time <= to) {
                return 1;
            }
        } else if (to == 0 && from == 0) {
            return 1;
        }
        holder = holder->next;        
    }
    for (int i = 0; i<comp->ncomps; i++) {
        toReturn = checkDate(comp->comp[i],from,to,caller);
        if (toReturn == 1) {
            return 1;
        }
    }     
    return toReturn;
}

CalProp * copyProp (CalProp * prop) {
    CalProp * propCopy;
    CalParam * paramHolder;
    CalParam * paramCopy;

    paramHolder = prop->param;

    propCopy = malloc(sizeof(CalProp));
    assert(propCopy != NULL);
    propCopy->name = malloc(sizeof(char)*strlen(prop->name)+1);
    assert(propCopy->name != NULL);
    propCopy->value = malloc(sizeof(char)*strlen(prop->value)+1); 
    assert(propCopy->value != NULL);
    propCopy->next = NULL;  
    strncpy(propCopy->name,prop->name,strlen(prop->name)+1);
    strncpy(propCopy->value,prop->value,strlen(prop->value)+1);
    propCopy->param = NULL;   
    while (paramHolder != NULL) {
        paramCopy = copyParam(paramHolder);
        addParam(propCopy,paramCopy);
        paramHolder = paramHolder->next;
        paramCopy = NULL;
    }
    return propCopy;
}

CalParam * copyParam (CalParam * param) {
    CalParam * paramCopy;

    paramCopy = malloc(sizeof(CalParam)+sizeof(char*)*param->nvalues);
    assert(paramCopy != NULL);
    paramCopy->name = malloc(sizeof(char)*strlen(param->name)+1);
    assert(paramCopy->name != NULL);
    paramCopy->nvalues = param->nvalues;
    paramCopy->next = NULL;
    strncpy(paramCopy->name,param->name,strlen(param->name)+1);
    for (int i = 0; i<paramCopy->nvalues; i++) {
        paramCopy->value[i] = malloc(sizeof(char)*strlen(param->value[i])+1);
        assert(paramCopy->value[i] != NULL);
        strncpy(paramCopy->value[i],param->value[i],strlen(param->value[i])+1);
    }
    return paramCopy;
}

void addParam (CalProp * prop, CalParam * param) {
    CalParam * nextP; 
 
    nextP = prop->param;
 
    if (nextP != NULL) {
        while (nextP->next != NULL) {
            nextP = nextP->next;
        }   
        nextP->next = param;
    } else {
        prop->param = param;
    }        
    prop->nparams++;
}

/*
Get comp details recursively
INPUT: component to search, cal details struct, nest level
OUTPUT: NA
*/
void countComp (CalComp * comp, InfoDetails * details, int level, char parent[MAX_XNAME]);

/*
Cycle through properties
INPUT: Component, cal details
OUTPUT: NA
*/
void thruPropsInfo (CalComp * comp, InfoDetails * details);

/*
Find CN value of organizer property
INPUT:property, string to store organizer
OUTPUT: success int
*/
int findCN (CalProp * prop, char ** organizer);

/*
Complementary function to qsort, needed to sort organizers
INPUT: Two organizer names
OUTPUT: int indicating compare result
*/
int nameCompare (const void * a, const void * b);

CalStatus calInfo(const CalComp * comp, int lines, FILE * const txtfile) {
    CalStatus toReturn;
    char toPrint[MAX_DATESTRING];
    char fromPrint[MAX_DATESTRING];
    InfoDetails details = {.events = 0, .todos = 0, .others = 0, .props = 0, 
      .subComps = 0, .orgSize = 0, .to = 0, .from = 0, .toStruct = NULL, .fromStruct = NULL};
    char lineBuilder[6] = "lines\0";
    char compBuilder[11] = "components\0";
    char eventBuilder[7] = "events\0";
    char todoBuilder[6] = "todos\0";
    char otherBuilder[7] = "others\0";
    char subBuilder[14] = "subcomponents\0";
    char propBuilder[11] = "properties\0";
    char orgHolder[MAX_ORGNAME] = {'\0'};
 
    details.organizers = malloc(sizeof(char *)*MAX_ORG);
    assert(details.organizers != NULL);
    details.toStruct = malloc(sizeof(struct tm*));
    assert(details.toStruct != NULL);
    details.fromStruct = malloc(sizeof(struct tm*));
    assert(details.fromStruct != NULL);
    details.toStruct[0] = NULL;
    details.fromStruct[0] = NULL;
    toReturn.code = OK;
    toReturn.lineto = 0;
    toReturn.linefrom = 0; 
    details.props = details.props + comp->nprops;
    for (int i = 0; i < comp->ncomps; i++) {
        countComp(comp->comp[i],&details,1,"CAL");
    } 
    //print info 
    if (lines == 1) {
        lineBuilder[4] = '\0';
    }
    if (comp->ncomps == 1) {
        compBuilder[9] = '\0';
    } 
    if (details.events == 1) {
        eventBuilder[5] = '\0';
    }
    if (details.todos == 1) {
        todoBuilder[4] = '\0';
    }
    if (details.others == 1) {
        otherBuilder[5] = '\0';
    }
    if (details.subComps == 1) {
        subBuilder[12] = '\0';
    }
    if (details.props == 1) {
        propBuilder[7] = 'y';
        propBuilder[8] = '\0';
        propBuilder[9] = '\0';
    }
    if (fprintf(txtfile,"%d %s\n",lines,lineBuilder) < 0) {
        toReturn.code = IOERR;
    } else {
        toReturn.lineto++; 
    }
    if (fprintf(txtfile,"%d %s: %d %s, %d %s, %d %s\n"
      ,comp->ncomps,compBuilder,details.events,eventBuilder,details.todos,todoBuilder,
      details.others,otherBuilder) < 0) {
        toReturn.code = IOERR;
    } else {
        toReturn.lineto++; 
    }  
    if (fprintf(txtfile,"%d %s\n",details.subComps,subBuilder) < 0) {
        toReturn.code = IOERR;
    } else {
        toReturn.lineto++; 
    }
    if (fprintf(txtfile,"%d %s\n",details.props,propBuilder) < 0) {
        toReturn.code = IOERR;
    } else {
        toReturn.lineto++; 
    }
    if (details.to == 0 && details.from == 0) {
        if (fprintf(txtfile,"No dates\n") < 0) {
            toReturn.code = IOERR;
        } else {
            toReturn.lineto++; 
        }
    } else {
        strftime(toPrint,MAX_DATESTRING,"%Y-%b-%d",details.toStruct[0]);
        strftime(fromPrint,MAX_DATESTRING,"%Y-%b-%d",details.fromStruct[0]);
        if (fprintf(txtfile,"From %s to %s\n",fromPrint,toPrint) < 0) {
            toReturn.code = IOERR;
        } else {
            toReturn.lineto++; 
        }
    }
    qsort(details.organizers,details.orgSize,sizeof(char*),nameCompare);
    if (details.orgSize != 0) {
        if (fprintf(txtfile,"Organizers:\n") < 0) {
            toReturn.code = IOERR;
        } else {
            toReturn.lineto++; 
        }
    } else {
        if (fprintf(txtfile,"No organizers\n") < 0) {
            toReturn.code = IOERR;
        } else {
            toReturn.lineto++; 
        }
    }
    for (int i = 0; i < details.orgSize; i++) {
        if (strcmp(orgHolder,details.organizers[i]) != 0) {
           if (fprintf(txtfile,"%s\n",details.organizers[i]) < 0) {
               toReturn.code = IOERR;
           } else {
               toReturn.lineto++; 
           } 
           strncpy(orgHolder,details.organizers[i],MAX_ORGNAME);
        }
    }
    //free
    for (int k = 0; k<details.orgSize; k++) {
        free(details.organizers[k]);
    } 
    free(details.organizers);
    if (details.toStruct[0] != NULL) {
        free(details.toStruct[0]);
    }
    free(details.toStruct);
    if (details.fromStruct[0] != NULL) {
        free(details.fromStruct[0]);
    }
    free(details.fromStruct); 
    toReturn.linefrom = toReturn.lineto;
    return toReturn;        
}

/*
Compare function for date qsort
INPUT: two dates to compare
OUTPUT: result of compare
*/
int eDateCompare (const void * a, const void * b);

/*
Sort X- props alphabetically
INPUT: two x-properties
OUTPUT: compare result
*/
int xCompare (const void * a, const void * b);

/*
Find all X- properties
INPUT: component to search, storage list for X-, number of props in storage
OUTPUT: New starage count
*/
int lookForX (const CalComp * comp, char ** list,int count);

CalStatus calExtract(const CalComp * comp, CalOpt kind, FILE * const txtfile) {
    CalStatus toReturn = {.code = OK, .lineto = 0, .linefrom = 0};
    ExtractEvent ** eventList;
    int eListCount = 0;
    CalProp * propHolder;
    char date[MAX_DATESTRING] = {'\0'};
    int xListCount = 0;
    char ** xList;
    char xHolder[MAX_XNAME] = {'\0'};
    
    if (kind == OEVENT) {
        eventList = malloc(sizeof(ExtractEvent*)*MAX_EVENTS);      
        assert(eventList != NULL);
        for (int i = 0; i<comp->ncomps; i++) {
            if (strcmp(comp->comp[i]->name,"VEVENT") == 0 && kind == OEVENT) {
                eventList[eListCount] = malloc(sizeof(ExtractEvent));
                assert(eventList[eListCount] != NULL);
                eventList[eListCount]->timeStruct = malloc(sizeof(struct tm*));
                assert(eventList[eListCount]->timeStruct != NULL);
                eventList[eListCount]->summary = malloc(sizeof(char)*MAX_SUMMARY);
                assert(eventList[eListCount]->summary != NULL);
                strncpy(eventList[eListCount]->summary,"\0",MAX_SUMMARY);
                propHolder = comp->comp[i]->prop;
                while (propHolder != NULL) {
                    if (strcmp(propHolder->name,"DTSTART") == 0) {
                         eventList[eListCount]->time = 
                           findDate(propHolder,eventList[eListCount]->timeStruct,EXTRACT);
                    }
                    if (strcmp(propHolder->name,"SUMMARY") == 0) {
                        strncpy(eventList[eListCount]->summary,propHolder->value,MAX_SUMMARY);
                    }
                    propHolder = propHolder->next;
                }
                if (strlen(eventList[eListCount]->summary) == 0) {
                    strncpy(eventList[eListCount]->summary,"(na)",MAX_SUMMARY);
                } 
                eListCount++;
            } 
        }
        qsort(eventList,eListCount,sizeof(ExtractEvent*),eDateCompare);
        for (int j = 0; j<eListCount; j++) {
            strftime(date,MAX_DATESTRING,"%Y-%b-%d %l:%M ",eventList[j]->timeStruct[0]);
            if (eventList[j]->timeStruct[0]->tm_hour > 11) {
                strcat(date,"PM");
            } else {
                strcat(date,"AM");
            }
            if (toReturn.code != IOERR) {
                if (fprintf(txtfile,"%s: %s\n",date,eventList[j]->summary) < 0) {
                    toReturn.code = IOERR;
                } else {
                    toReturn.lineto++;
                }
            }
            free(eventList[j]->timeStruct[0]);
            free(eventList[j]->timeStruct);
            free(eventList[j]->summary);
            free(eventList[j]);
        }
        free(eventList);
    } else {
        xList = malloc(sizeof(char*)*MAX_XPROPS);
        assert(xList != NULL);
        xListCount = lookForX(comp,xList,xListCount);
        qsort(xList,xListCount,sizeof(char*),xCompare);
        for (int k = 0; k<xListCount; k++) {
            if (toReturn.code == OK && strcmp(xList[k],xHolder) != 0) {
                if (fprintf(txtfile,"%s\n",xList[k]) < 0) {
                    toReturn.code = IOERR;
                } else {
                    toReturn.lineto++;
                }
                strncpy(xHolder,xList[k],MAX_XNAME);
            }
            free(xList[k]);
        }
        free(xList); 
    }
    toReturn.linefrom = toReturn.lineto;
    return toReturn;
}

int lookForX (const CalComp * comp, char ** list,int count) {
    CalProp * propHolder;
    int addToCount = count;

    propHolder = comp->prop;
    while (propHolder != NULL) {
        if (propHolder->name[0] == 'X' && propHolder->name[1] == '-') {
            list[addToCount] = malloc(sizeof(char)*MAX_XNAME);
            assert(list[addToCount] != NULL);
            strncpy(list[addToCount],propHolder->name,MAX_XNAME);
            addToCount++;
        }
        propHolder = propHolder->next;
    }
    for (int i = 0; i<comp->ncomps; i++) {
        addToCount = lookForX(comp->comp[i],list,addToCount);
    }
    return addToCount;
}

int xCompare (const void * a, const void * b) {
    return strcmp(*(char**)a,*(char**)b);
}

int eDateCompare (const void * a, const void * b) {
    time_t dateA,dateB = 0;

    ExtractEvent ** toCompA = (ExtractEvent**)a;
    ExtractEvent ** toCompB = (ExtractEvent**)b;
    dateA = toCompA[0]->time;
    dateB = toCompB[0]->time;

    if (dateA > dateB) {
        return 1;
    } else if (dateA < dateB) {
        return -1;
    } else {
        return 0;
    }
}

time_t findDate(CalProp * prop, struct tm ** timeStruct, ComType caller) {

    timeStruct[0] = malloc(sizeof(struct tm));
    assert(timeStruct[0] != NULL);
    timeStruct[0]->tm_sec = 0;
    timeStruct[0]->tm_min = 0;
    timeStruct[0]->tm_hour = 0;
    timeStruct[0]->tm_mday = 1;
    timeStruct[0]->tm_mon = 0;
    timeStruct[0]->tm_year = 0; 

    if (strcmp(prop->name,"COMPLETED") == 0 || strcmp(prop->name,"DTEND") == 0 ||
      strcmp(prop->name,"DUE") == 0 || strcmp(prop->name,"DTSTART") == 0 ||
      (strcmp(prop->name,"CREATED") == 0 && caller != FILTER) || (strcmp(prop->name,"DTSTAMP") == 0 
      && caller != FILTER) || (strcmp(prop->name,"LAST-MODIFIED") == 0 && caller != FILTER)) {
        strptime(prop->value,"%Y%m%dT%H%M%S",(*timeStruct));
        timeStruct[0]->tm_isdst = -1;
        return mktime((*timeStruct));
    } else {
        return 0;
    } 
}

int nameCompare (const void * a, const void * b) {
    char charA,charB;

    char * toCompA = *(char**)a;
    char * toCompB = *(char**)b;
    charA = tolower(toCompA[0]);
    charB = tolower(toCompB[0]);

    if (charA > charB) {
        return 1;
    } else if (charA < charB) {
        return -1;
    } else {
        return 0;
    }
}

void countComp (CalComp * comp, InfoDetails * details, int level, char parent[MAX_XNAME]) {
    if (level == 1) {
        if (strcmp(comp->name,"VEVENT") == 0) {
            details->events = details->events + 1;
        } else if (strcmp(comp->name,"VTODO") == 0) {
            details->todos  = details->todos + 1;
        } else {
            details->others = details->others + 1;
        }
    }
    details->subComps = details->subComps + comp->ncomps;
    details->props = details->props + comp->nprops;
    if (strcmp(parent,"VTIMEZONE") != 0) {
        thruPropsInfo(comp,details);
    }
    for (int i = 0; i<comp->ncomps; i++) {
        countComp(comp->comp[i],details,level+1,comp->name);
    }
}

void thruPropsInfo (CalComp * comp, InfoDetails * details) {
    CalProp * holder;
    char * orgToAdd;
    time_t time;
    struct tm * timeStruct;

    holder = comp->prop;
    orgToAdd = NULL;
    while (holder != NULL) {
        timeStruct = NULL; 
        if (strcmp(holder->name,"ORGANIZER") == 0) {
            if(findCN(holder,&orgToAdd) == 1) {
                details->organizers[details->orgSize] = orgToAdd;
                details->orgSize = details->orgSize + 1;
            } 
        }
        time = findDate(holder,&timeStruct,INFO);
        if (time > 0 && details->from == 0 && details->from == 0) {
            details->to = time;
            details->from = time;
            details->toStruct[0] = timeStruct;
            details->fromStruct[0] = timeStruct;
        } else if (time > details->to) {
            if (details->fromStruct[0] != details->toStruct[0]) {
                free(details->toStruct[0]);
            }
            details->to = time;
            details->toStruct[0] = timeStruct;
        } else if (time < details->from && time != 0) {
            if (details->fromStruct[0] != details->toStruct[0]) {
                free(details->fromStruct[0]);
            }
            details->from = time;
            details->fromStruct[0] = timeStruct;
        } else {
            free(timeStruct);
        }        
        holder = holder->next;
    }
}

int findCN (CalProp * prop, char ** organizer) {
    CalParam * holder;

    holder = prop->param;

    while (holder != NULL) {
        if (strcmp(holder->name,"CN") == 0) {
            *organizer = malloc(sizeof(char)*(strlen(holder->value[0])+1));
            assert(*organizer != NULL);
            strncpy(*organizer,holder->value[0],strlen(holder->value[0])+1);
            return 1;
        }
        holder = holder->next;
    }
    return 0;
}
