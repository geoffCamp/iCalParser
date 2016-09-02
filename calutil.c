/*********
calutil.c -- Private utility functions for iCalendar
Last Modifieded:  3:30 PM Feb-22-16
Geofferson Camp (gcamp@mail.uoguelph.ca)
0658817

writeCalComp added for A2
********/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "calutil.h"
#include <assert.h>

#define BUFF_SIZE 6000
#define NAME_SIZE 30
#define SYM_SIZE 100
#define WRITE_GOOD 1
#define WRITE_BAD -1

/*
Print param
INPUTS: CalParam
OUTPUT: NA
*/
void printParam (char ** buff, CalParam * param);

/*
Print property
INPUTS: CalProp
OUTPUT: NA
*/
void printProp (char ** buff,CalProp * prop);

CalStatus writeCalComp (FILE * const ics, const CalComp * comp) {
    static CalStatus toReturn = {.code = OK, .lineto = 0, .linefrom = 0};
    CalProp * holder;
    char ** buff;
    char tempBuffer[BUFF_SIZE] = {'\0'};
    int foldCount = 0;

    buff = malloc(sizeof(char*));
    assert(buff != NULL);
    holder = comp->prop;
    if (fprintf(ics,"BEGIN:%s\r\n",comp->name) < 0) {
        toReturn.code = IOERR;
        toReturn.linefrom = toReturn.lineto;
        free(buff);
        return toReturn;
    }
    toReturn.lineto++;
    toReturn.linefrom = toReturn.lineto;
    //cycles through properties
    while (holder != NULL) {
        buff[0] = calloc(BUFF_SIZE,sizeof(char)*BUFF_SIZE);
        assert(buff[0] != NULL);
        printProp(buff,holder);
        while (strlen(buff[0])-2 > (foldCount+1)*FOLD_LEN) { 
            if (strlen(tempBuffer) == 0) {
                strncpy(tempBuffer,buff[0],FOLD_LEN);
            } else {
                if (foldCount == 1) {
                    strncat(tempBuffer,buff[0]+((FOLD_LEN)*foldCount),FOLD_LEN-1);            
                } else {
                    strncat(tempBuffer,buff[0]+((FOLD_LEN-1)*foldCount)+1,FOLD_LEN-1);
                }
                
            } 
            strcat(tempBuffer,"\r\n ");
            foldCount++;
            toReturn.lineto++;
        }
        if (foldCount > 0) {
            strncat(tempBuffer,buff[0]+((FOLD_LEN-1)*foldCount)+1,FOLD_LEN+3);
            //strcat(tempBuffer,"\r\n\0");
            strncpy(buff[0],tempBuffer,BUFF_SIZE);
            foldCount = 0;
            for (int j = 0; j < BUFF_SIZE; j++) {
                tempBuffer[j] = '\0';
            }
        }       
        if (fprintf(ics,"%s",buff[0]) < 0) {
            toReturn.code = IOERR;
            if (toReturn.lineto == toReturn.linefrom+1) {
                toReturn.lineto--;
            }
            toReturn.linefrom = toReturn.lineto;
            free(buff[0]);
            free(buff);
            return toReturn;
        }
        free(buff[0]);
        toReturn.lineto++;
        toReturn.linefrom = toReturn.lineto;
        holder = holder->next;
    }
    //cycles through comps
    for (int i = 0; i < comp->ncomps; i++) {
        toReturn = writeCalComp(ics,comp->comp[i]);
        if (toReturn.code == IOERR) {
            toReturn.linefrom = toReturn.lineto;
            free(buff);
            return toReturn;
        }
    }
    if (fprintf(ics,"END:%s\r\n",comp->name) < 0) {
        toReturn.code = IOERR;
        toReturn.linefrom = toReturn.lineto;
        free(buff);
        return toReturn;
    }
    toReturn.lineto++;
    toReturn.linefrom = toReturn.lineto;
    free(buff);
    return toReturn;   
}

void printParam (char ** buff,CalParam * param) {

    strcat(buff[0],";");
    strcat(buff[0],param->name);
    strcat(buff[0],"="); 
    for (int i = 0; i<param->nvalues; i++) {
        if (i != 0) {
            strcat(buff[0],",");
        }
        strcat(buff[0],param->value[i]);
    }
}

void printProp (char ** buff,CalProp * prop) {
    CalParam * forLoop;

    strncpy(buff[0],prop->name,BUFF_SIZE);
    forLoop = prop->param;
    while (forLoop != NULL) {
        printParam(buff,forLoop);
        forLoop = forLoop->next;
    }
    strcat(buff[0],":");
    strcat(buff[0],prop->value);
    strcat(buff[0],"\r\n");
}

/*
Initialize CalComp components
INPUT: Component double pointer, name of component
OUTPUT: NA
*/
void initCalComp (CalComp ** comp, char name[NAME_SIZE]);


/*
Check component for required VERSION and PRODID specs
INPUT: Component
OUTPUT: CalError
*/
CalError checkReq (CalComp * comp);

/*
check calendar for required components
INPUT: component
OUTPUT: CalError
*/
CalError checkComps (CalComp * comp);

CalStatus readCalFile(FILE *const ics, CalComp **const pcomp) {
    CalStatus toReturn;

    toReturn = readCalLine(NULL,NULL);
    initCalComp(pcomp,NULL);    
    toReturn = readCalComp(ics,pcomp);   
    if (toReturn.code == OK) {
        toReturn.code = checkReq((*pcomp));
    }
    if (toReturn.code == OK) {
        toReturn.code = checkComps((*pcomp));
    }
    if (toReturn.code != OK) {
        freeCalComp((*pcomp));
    }
    if (toReturn.linefrom == 0 && toReturn.lineto == 0) {
        toReturn.code = NOCAL;
    }
    return toReturn;
}

/* (see free section) */
void freeProp (CalProp * prop);

/*
Add component to parent component
INPUT: parent CalComp, CalComp to add to parent
OUTPUT: NA
*/
void addComp(CalComp ** rootComp, CalComp * compAdding);

CalStatus readCalComp(FILE *const ics, CalComp **const pcomp) {
    MallocStatus propToAddStatus;
    MallocStatus nextCompStatus;
    static int nestLevel;
    CalStatus status;
    char ** pbuff;
    CalComp ** nextComp;// next component to add
    CalError parseError;
    CalProp * propToAdd;
    char endCondition[BUFF_SIZE];//stores end condition to break out of component

    propToAddStatus = NOTHING;
    nextCompStatus = NOTHING;
    pbuff = malloc(sizeof(char *));
    assert(pbuff != NULL);
    nextComp = malloc(sizeof(CalComp*));
    assert(nextComp != NULL);
    nextCompStatus = MALLOCED;
    status.code = OK;
    status.lineto = 1;
    status.linefrom = 1;

    //first component set up
    if (!pcomp[0]->name) {
        nestLevel = 1;
        pcomp[0]->name = malloc(sizeof(char)*NAME_SIZE);
        assert(pcomp[0] != NULL);
        status = readCalLine(ics,pbuff);
        propToAdd = malloc(sizeof(CalProp));
        assert(propToAdd != NULL);
        propToAddStatus = MALLOCED;
        if (pbuff[0] != NULL) {
            parseError = parseCalProp(pbuff[0],propToAdd);
            free(pbuff[0]);
            propToAddStatus = INNERFREEABLE;
        } else {
            parseError = status.code;
        }
        status.code = parseError;
        if (status.code == SYNTAX) {
        } else if (status.code == NOCRNL) {
        } else if (strcmp(propToAdd->name,"BEGIN") == 0 && strcmp(propToAdd->value,"VCALENDAR")==0) {
            strcpy(pcomp[0]->name,"VCALENDAR");
        } else {
            status.code = NOCAL;
        }
        if (propToAddStatus == INNERFREEABLE) {
            freeProp(propToAdd);
        } else {
            free(propToAdd);
        }
        propToAddStatus = ADDED;
    }
    if (status.code != NOCAL ) {
        strcpy(endCondition,(*pcomp)->name);
    }
    //read through file. create components and properties when appropriate.
    while (!(status.code != OK && pbuff != NULL) && status.code == OK) {
        status = readCalLine(ics,pbuff);
        if ((status.code == OK) && (pbuff[0] == NULL || strcmp(pbuff[0],"")==0)) {
            status.lineto--;
            status.linefrom--;
            status.code = BEGEND;
            break;
        }
        if (status.code == OK) {
            propToAdd = malloc(sizeof(CalProp));
            assert(propToAdd != NULL);
            propToAddStatus = MALLOCED;
            parseError = parseCalProp(pbuff[0],propToAdd);
            free(pbuff[0]);
            status.code = parseError;
            if (status.code == SYNTAX) {
                break;
            }
            propToAddStatus = INNERFREEABLE;
            if (strcmp(propToAdd->name,"BEGIN") == 0) {
                nestLevel++;
                if (nestLevel < 4) {
                    initCalComp(nextComp,propToAdd->value);
                    nextCompStatus = INNERFREEABLE;
                    status = readCalComp(ics,nextComp);
                    if (status.code != OK) { 
                        break;
                    }
                    addComp(pcomp,nextComp[0]);
                    nextCompStatus = ADDED;                  
                } else {
                    status.code = SUBCOM;
                    break;           
                }
                nestLevel--;
                freeProp(propToAdd);
                propToAddStatus = ADDED; //so no one tries to free it
            } else if (strcmp(propToAdd->name,"END")==0) {         
                if ((*pcomp)->nprops == 0 && (*pcomp)->ncomps == 0) {
                    status.code = NODATA;
                } else if (strcmp(endCondition,propToAdd->value)==0) {
                    if (strcmp(propToAdd->value,"VCALENDAR")==0) {
                        if (!feof(ics)) {
                            readCalLine(ics,pbuff);
                            status.code = AFTEND;
                            status.linefrom = status.linefrom+1;
                            status.lineto = status.lineto+1;
                        }
                    }
                } else {
                    status.code = BEGEND;
                }
                freeProp(propToAdd);
                propToAddStatus = ADDED; //so no one tries to free it
                break;
            } else { //property to be added (default)
                 addProp((*pcomp),propToAdd);
                 propToAddStatus = ADDED;       
            }    
        } else if (status.code == NOCRNL) { //improper carriage
             break; 
        }
    }
    if (propToAddStatus != ADDED) {
        if (propToAddStatus == MALLOCED) {
            free(propToAdd);
        } else if (propToAddStatus == INNERFREEABLE) {
            freeProp(propToAdd);
        }
    }
    if (nextCompStatus != ADDED) {
       if (nextCompStatus == INNERFREEABLE) {
            freeCalComp((*nextComp));
        }
    }
    free(nextComp);
    free(pbuff); 
    return status;
}

/*
Removes blanks from lines to be appended
INPUT: char array
OUTPUT: NA 
*/
void clearBlanks(char * string);

/*
Append lines starting with a blank space to previous line 
INPUT: two char arrays
OUTPUT: NA
*/
void append (char * string1, char * string2);

/*
Checks for EOL requirements
INPUT: char array
OUTPUT: CalError indicating if EOL conditions are met
*/
CalError checkEOL (char * line, FILE * file);

/*
Removes EOL characters from lines
INPUT: char array
OUTPUT: boolean indicating success
*/
bool clearEOLChars (char * line);

/*
Checks if the provided line is blank
INPUT: char array
OUTPUT: int indicating blank or not blank
*/
int notBlank(char * temp);

/*
Copy part of a string to destination
INPUT: Destination, source, start and end positions of string to copy
OUTPUT: NA
*/
void copySubStr (char * dest, char * src, int start, int end);

CalStatus readCalLine(FILE *const ics, char **const pbuff) {
    char * temp;;
    static char buffer[BUFF_SIZE];
    static int lineNumber;
    CalStatus toReturn;   
    static bool EOFb4EOL;
    int blanksSkipped;

    blanksSkipped = 0; 

    //reset static variables
    if (ics == NULL) {
        lineNumber = 0;
        toReturn.code = OK;
        toReturn.linefrom = 0;
        toReturn.lineto = 0;
        return toReturn;
    }

    pbuff[0] = malloc(sizeof(char)*BUFF_SIZE);
    assert(pbuff[0] != NULL);

    //check for EOF conditions
    if (feof(ics)) {
        toReturn.code = OK;
        toReturn.lineto = lineNumber+1;
        toReturn.linefrom = lineNumber+1;
        if (EOFb4EOL == false) {
            pbuff[0] = NULL;
        } else {
            EOFb4EOL = false;
            copySubStr(pbuff[0],buffer,0,BUFF_SIZE-2);
        }
        return toReturn;
    }
    temp = calloc(BUFF_SIZE,sizeof(char)*BUFF_SIZE);
    assert(temp != NULL);
    if (lineNumber == 0) {
        fgets(buffer,BUFF_SIZE,ics);
    }
    toReturn.code = checkEOL(buffer,ics); 
    clearEOLChars(buffer);       
    if (fgets(temp,BUFF_SIZE,ics)) {
    } else {
        //fprintf(stderr,"read error\n");
    }
    lineNumber++;
    toReturn.linefrom = lineNumber;
    while (notBlank(temp) == 0 && !feof(ics)) {
        fgets(temp,BUFF_SIZE,ics);
        blanksSkipped++;
    }
    if (feof(ics)) {
        EOFb4EOL = true;
    } else { 
        EOFb4EOL = false;
    }
    if (!isspace(temp[0])) {
        copySubStr(pbuff[0],buffer,0,BUFF_SIZE-2);
        copySubStr(buffer,temp,0,BUFF_SIZE-2);
        toReturn.lineto = lineNumber;// + blanksSkipped;
        lineNumber = lineNumber + blanksSkipped;
    } else if (isspace(temp[0])) {
        lineNumber = lineNumber + blanksSkipped;
        blanksSkipped = 0;
        while (isspace(temp[0])) {
            //this maintains a NOCRNL status while allowing OK to change to NOCRNL
            if (toReturn.code == OK ) {
                toReturn.code = checkEOL(temp,ics);
            }; 
            clearEOLChars(temp);
            append(buffer,temp);
            if (!fgets(temp,BUFF_SIZE,ics)) {
            }
            lineNumber++;
            while (notBlank(temp) == 0 && !feof(ics)) {
                fgets(temp,BUFF_SIZE,ics);
                lineNumber++;
            }
            if (feof(ics)) {
                break;
            }
        }
        toReturn.lineto = lineNumber;
        copySubStr(pbuff[0],buffer,0,BUFF_SIZE-2);
        copySubStr(buffer,temp,0,BUFF_SIZE-2);
    }
    if (toReturn.code == NOCRNL) {
        pbuff[0] = NULL;
    }
    free(temp);
    return toReturn;
}

/*
Initialize new parameter to add to active property
INPUT: CalParam
OUTPUT: NA
*/
void initParam (CalParam * newParam);

/*
Convert parameter names to upper case
INPUT: char array
OUTPUT: NA
*/
void stringToUpper(char * string);

/*
Check for unexpected white space
INPUT: char array
OUTPUT: int indicating white space or not
*/
int checkForSpace (char * string);

CalError parseCalProp(char * const buff, CalProp * const prop) {
    CalError toReturn;
    char * foundChar;   
    char active;
    int quoteOn;
    int colonOn;
    int to;
    int from;
    char symFound[SYM_SIZE];
    int foundCount;
    CalParam * newParam;
    CalParam * nextParam;
    int nEqual = 0;
    int nQuote = 0;
    int nSemi = 0;
    int nColon = 0;

    toReturn = OK;
    quoteOn = 0;
    colonOn = 0;
    to = 0;
    from = 0;
    foundCount = 0;
    prop->nparams = 0;
    prop->name = NULL;
    prop->value = NULL;
    prop->param = NULL;
    prop->next = NULL;

    for (int i = 0; i<strlen(buff)+1; i++) {
        active = buff[i];
        foundChar = strchr(";:=\",\0",active);
        to++;
        if (foundChar == NULL || (colonOn == 1 && active != '\0' ) || (quoteOn == 1 && active != '"')) {
            continue;
        } else {
            symFound[foundCount] = active;
            symFound[foundCount+1] = '\0';
            foundCount++;
        }
        if (active == '"') {
           if (quoteOn == 0) {
               quoteOn = 1;
               from = i-1;
           } else {
               quoteOn = 0;
               continue;
           }
        }
        if (active == ':') {
           if (colonOn == 0) {
               colonOn = 1;
           }
        } 
        //skip symbol processing if in a quoted string or prop value
        if (quoteOn == 1 || (colonOn == 1 && (active != '\0' && active != ':'))) {
            continue;
        }
        //set property value 
        if (active == '\0') {
            prop->value = malloc(sizeof(char)*(to-from+1));
            assert(prop->value != NULL);
            copySubStr(prop->value,buff,from+1,to); 
            if (prop->name != NULL) {
                if ((strcmp(prop->name,"BEGIN")==0 || strcmp(prop->name,"END") == 0) 
                  && checkForSpace(prop->value) == 1) {
                    toReturn = SYNTAX;
                }
                if (strcmp(prop->name,"BEGIN")==0 || strcmp(prop->name,"END") == 0) {
                    stringToUpper(prop->value);
                }   
            } 
        }
        //set prop name
        if (foundCount == 1 && (active == ':' || active == ';')) {
            prop->name = malloc(sizeof(char)*(to-from+1)); 
            assert(prop->name != NULL);
            copySubStr(prop->name,buff,from,to-2);
            stringToUpper(prop->name);
            if (checkForSpace(prop->name) == 1) {
                toReturn = SYNTAX;
            }
        }
        //set newParam name
        if (active == '=') {
            newParam->name = malloc(sizeof(char)*(to-from+1));
            assert(newParam->name != NULL);
            copySubStr(newParam->name,buff,from+1,to-2);
            stringToUpper(newParam->name);
            if (strcmp(newParam->name,"") == 0 || checkForSpace(newParam->name) == 1) {
                toReturn = SYNTAX;
            }
        }
        //set a new param value
        if ((active == ',') || ((active == ';' || active == ':') && 
          (symFound[foundCount-2] == '=' || symFound[foundCount-2] == ','
          || symFound[foundCount-2] == '"')) ) { 
            newParam->nvalues++;
            newParam = realloc(newParam,sizeof(CalParam) + sizeof(char*)*newParam->nvalues);
            assert(newParam != NULL);
            newParam->value[newParam->nvalues-1] = malloc(sizeof(char)*(to-from+1));
            assert(newParam->value[newParam->nvalues-1] != NULL);
            copySubStr(newParam->value[newParam->nvalues-1],buff,from+1,to-2); 
            if (newParam->value[newParam->nvalues-1][0] != '"') {
                //stringToUpper(newParam->value[newParam->nvalues-1]);
            }   
        }
        //add newParam to prop
        if ((active == ';' || active == ':' || active == '=') && 
          (symFound[foundCount-2] == ',' || symFound[foundCount-2] == '=' || 
            symFound[foundCount-2] == '"' ) ) {
            nextParam = prop->param;
            if (nextParam != NULL) {
                while (nextParam->next != NULL) {
                    nextParam = nextParam->next;
                }
                nextParam->next = newParam;
            } else {
                prop->param = newParam;
            }
            prop->nparams++;
        } 
        //initialize new parameter 
        if (active == ';') {
            newParam = NULL;
            newParam = malloc(sizeof(CalParam)+sizeof(char*)); 
            assert(newParam != NULL);
            initParam(newParam);
        }
        from = i;
    }
    //count symbols for syntax check
    for (int j = 0; j<strlen(symFound); j++) {
        switch (symFound[j]) {
            case ';':
                nSemi++;
                break;
            case ':':
                nColon++;
                break;
            case '"':
                nQuote++;
                break;
            case '=':
                nEqual++;
                break;
        }
    }
    //screen count data for SYNTAX
    if (nColon < 1) {
        toReturn = SYNTAX;
    }
    if (nEqual != nSemi || (nQuote % 2) != 0) {
        toReturn = SYNTAX;
    }
    if (buff[0] == ':' || buff[0] == ';' || buff[0] == '"' || buff[0] == '=') {
        toReturn = SYNTAX;
    }
    return toReturn;
}

/*
Free all dynamic memory associated with a paramter
INPUT: CalParam
OUTPUT: NA
*/
void freeParam (CalParam * param);

/*
Free all dynamic memory associated with a property
INPUT: CalProp
OUTPUT: NA
*/
void freeProp (CalProp * prop);

void freeCalComp(CalComp *const comp) {
    CalProp * nextProp;
    CalProp * holder;
 
    free(comp->name);
    holder = comp->prop;
    while (holder != NULL) {
        nextProp = holder->next;
        freeProp(holder);
        holder = nextProp;
    }
    for (int i = 0; i < comp->ncomps; i++) {
        freeCalComp(comp->comp[i]);
    }
    free(comp);
}

/* 
support functions
*/
/* readCalFile */
void initCalComp (CalComp ** comp, char name[NAME_SIZE]) {

    comp[0] = malloc(sizeof(CalComp) + sizeof(CalComp *));
    assert(comp[0] != NULL);
    if (!name) {
        comp[0]->name = NULL;
    } else {
        comp[0]->name = malloc(sizeof(char)*NAME_SIZE);
        assert(comp[0]->name != NULL);
        strcpy(comp[0]->name,name);
    }
    comp[0]->nprops = 0;
    comp[0]->prop = NULL;
    comp[0]->ncomps = 0;
}

CalError checkReq (CalComp * comp) {
    CalProp * prop;
    CalError error;
    int foundProdId;
    int foundVersion;
    char versionVal[100];
    
    foundProdId = 0;
    foundVersion = 0;
    prop = comp->prop;
    //search for occurences of PRODID and VERSION
    while (prop != NULL) {
        if (strcmp(prop->name,"PRODID")==0) {
            foundProdId++;         
        }
        if (strcmp(prop->name,"VERSION")==0) {
            foundVersion++;
            strcpy(versionVal,prop->value);
        }
        prop = prop->next;
    }
    //set return value based on search
    if (foundProdId != 1 && foundVersion != 1) {
        error = BADVER; 
    } else if (foundVersion != 1 || strcmp(VCAL_VER,versionVal) != 0) {
        error = BADVER;
    } else if (foundProdId != 1) {
        error = NOPROD;
    } else {
        error = OK;
    }
    return error;
}

CalError checkComps (CalComp * comp) {
    CalError vCheck;
    CalError numCheck;

    vCheck = NOCAL;
    numCheck = NOCAL;

    for (int i = 0; i<comp->ncomps; i++) {
        if (comp->comp[i]->name[0] == 'V') {
            vCheck = OK;
        }
        if (comp->ncomps > 0) {
            numCheck = OK;
        }
    }
    if (vCheck == OK && numCheck == OK) {
        return OK;
    } else { 
        return NOCAL;
    }
}

/* readCalComp */
void addComp(CalComp ** rootComp, CalComp * compAdding) {
    (*rootComp)->ncomps++;
    (*rootComp) = realloc((*rootComp),sizeof(CalComp)+sizeof(CalComp*)*((*rootComp)->ncomps+1));
    assert((*rootComp) != NULL);
    (*rootComp)->comp[(*rootComp)->ncomps-1] = compAdding;
}

void addProp(CalComp * comp, CalProp * prop) {
    CalProp * nextP;

    nextP = comp->prop;
    if (nextP != NULL) {
        while (nextP->next != NULL) {
            nextP = nextP->next;
        }
        nextP->next = prop;
    } else {
        comp->prop = prop;
    } 
    comp->nprops++;
}

/* readCalLine */
int notBlank(char * temp) {
    int length;
    int notBlank;

    notBlank = 0;
    if (temp[0] == ' ') {
        return 1;
    }
    length = strlen(temp);
    for (int i = 0; i<length; i++) {
        if (temp[i] != ' ' && temp[i] != '\r' && temp[i] != '\n' && temp[i] != '\t') {
            notBlank = 1;
            break;
        }
    }
    return notBlank;
}

bool clearEOLChars (char * line) {
    int initLength;

    initLength = strlen(line);
    line = strtok(line,"\r\n");
    if (line == NULL) {
        return true;
    }
    if (initLength-2 == strlen(line)) {
        return true;
    } else {
        return false;
    }
}

CalError checkEOL (char * line, FILE * file) {
    int length;
    length = strlen(line);

    if ((line[length-1] != '\n' || line[length-2] != '\r') && !feof(file)) { //'\r\n'
        return NOCRNL;
    } else {
        return OK;
    }
}

void append (char * string1, char * string2) {
    string1[strlen(string1)] = '\0';
    clearBlanks(string2);
    strcat(string1,string2);
}

void clearBlanks(char * string) {
    for (int i = 0; i<=strlen(string); i++) {
        string[i] = string[i+1];
    }
}

/* parseCalComp */
void copySubStr (char * dest, char * src, int start, int end) {
    for (int i = 0; i<=end-start; i++) {
        dest[i] = src[start+i];
    }
    dest[end-start+1] = '\0';
}

int checkForSpace (char * string) {
    for (int i = 0; i < strlen(string); i++) {
        if (isspace(string[i])) {
            return 1;
        }
    }
    return 0;
}

void stringToUpper(char * string) {
    for (int i = 0; i < strlen(string); i++) {
        string[i] = toupper(string[i]);
    }
}

void initParam (CalParam * newParam) {    
    newParam->name = NULL;
    newParam->next = NULL;
    newParam->nvalues = 0;
    newParam->value[0] = NULL;
}

/* calCompFree */
void freeProp (CalProp * prop) {
    CalParam * nextParam;
    CalParam * holder;

    holder = prop->param;
    free(prop->name);
    free(prop->value);

    while (holder != NULL) {
        nextParam = holder->next;
        freeParam(holder);
        free(holder);
        holder = nextParam;
    }
    free(prop);
}

void freeParam (CalParam * param) {   
    free(param->name);
    for (int i = 0; i < param->nvalues; i++) {
        free(param->value[i]);
    }
}
