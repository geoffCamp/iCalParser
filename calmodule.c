/******************
calmodule.c - xcal library for python
Last Modified: 330pm April 6, 2016
Geofferson Camp (gcamp@mail.uoguelph.ca)
0658817

- Added ability to parse organizers, locations, and priorities for A4 
********************/

#include <Python.h>
#include "calutil.h"

static PyObject * Cal_readFile(PyObject * self, PyObject * args);
static PyObject * Cal_writeFile(PyObject * self, PyObject * args);
static PyObject * Cal_freeFile(PyObject * self, PyObject * args);

//list of methods being exported
static PyMethodDef CalMethods[] = {
    {"readFile", Cal_readFile, METH_VARARGS, "opens file and returns pointer to cal"},
    {"writeFile", Cal_writeFile, METH_VARARGS, "writes calComps to file"},
    {"freeFile", Cal_freeFile, METH_VARARGS, "frees previously read iCal file"},
    {NULL, NULL, 0, NULL}, 
};

//module header definition
static struct PyModuleDef CalModule = {
    PyModuleDef_HEAD_INIT,
    "CalModule",
    NULL,
    -1,
    CalMethods   
};

PyMODINIT_FUNC
PyInit_CalModule (void) {
    return PyModule_Create(&CalModule);
}

/*
Check if the component is in compList
*/
int checkIn (int index, PyObject * compList);

/*
Remove nulls from component comp list
*/
void removeNulls (CalComp * comp);

/*
Convert PyObject to integer
*/
int pyToInt (PyObject * intObj);

//wrapper functions
static PyObject * Cal_readFile (PyObject * self, PyObject * args) {
    char * filename;
    PyObject * result;
    CalComp * cal = NULL;
    FILE * file;
    PyObject * temp;
    CalProp * holder = NULL;
    CalProp * sumHolder = NULL;
    CalProp * orgHolder = NULL;
    CalParam * CNparam = NULL;
    CalProp * seventhHolder = NULL;
    CalProp * locHolder = NULL;
    int found,foundOrg,foundLoc,foundSeventh;

    if (PyTuple_Size(args) == 2 && PyArg_ParseTuple(args,"sO",&filename,&result)) {
        file = fopen(filename,"r");
        readCalFile(file,&cal);
        temp = Py_BuildValue("k",cal);
        PyList_Append(result, temp);
        for (int i = 0; i < cal->ncomps; i++) {
            found = 0;
            foundOrg = 0;
            foundLoc = 0;
            foundSeventh = 0;
            temp = Py_None;
            temp = Py_BuildValue("s",cal->comp[i]->name);
            PyList_Append(result,temp);
            temp = Py_None;
            temp = Py_BuildValue("i",cal->comp[i]->nprops);
            PyList_Append(result,temp);
            temp = Py_None;
            temp = Py_BuildValue("i",cal->comp[i]->ncomps);
            PyList_Append(result,temp);
            holder = cal->comp[i]->prop;
            while (holder != NULL) {
                if (strcmp(holder->name,"SUMMARY") == 0) {
                    found = 1;
                    sumHolder = holder;
                } else if (strcmp(holder->name,"ORGANIZER") == 0) {
                    foundOrg = 1;
                    orgHolder = holder;
                } else if (strcmp(holder->name,"PRIORITY") == 0 && 
                  strcmp("VTODO",cal->comp[i]->name) == 0) {
                    seventhHolder = holder;
                    foundSeventh = 1;
                } else if (strcmp("DTSTART",holder->name) == 0) {
                    foundSeventh = 1;
                    seventhHolder = holder;
                } else if (strcmp("LOCATION",holder->name) == 0) {
                    foundLoc = 1;
                    locHolder = holder;
                }
                holder = holder->next;
            }
            temp = Py_None;
            if (found == 1) {
                temp = Py_BuildValue("s",sumHolder->value);
            } else {
                temp = Py_BuildValue("s","");
            }
            PyList_Append(result,temp);
 
            //find organizer common name
            temp = Py_None;
            if (foundOrg == 1) {
                CNparam = orgHolder->param;
                while (CNparam != NULL) {
                    if (strcmp(CNparam->name,"CN") == 0) {
                        break;
                    }
                    CNparam = CNparam->next;
                }
                if (CNparam != NULL) {
                    temp = Py_BuildValue("s",CNparam->value[0]);
                } else {
                    temp = Py_BuildValue("s","");
                }
            } else {
                temp = Py_BuildValue("s","");
            }
            PyList_Append(result,temp);

            //find organizer contact
            temp = Py_None;
            if (foundOrg == 1) {
                temp = Py_BuildValue("s",orgHolder->value);
            } else {
                temp = Py_BuildValue("s","");
            }
            PyList_Append(result,temp);

            //set priority or dtstart
            temp = Py_None;
            if (foundSeventh == 1) {
                temp = Py_BuildValue("s",seventhHolder->value);
            } else {
                temp = Py_BuildValue("s","");
            }
            PyList_Append(result,temp);

            //set location contact
            temp = Py_None;
            if (foundLoc == 1) {
                temp = Py_BuildValue("s",locHolder->value);
            } else {
                temp = Py_BuildValue("s","");
            }
            PyList_Append(result,temp);

        }
        fclose(file);
        return Py_BuildValue ("s", "OK");
    }
    return NULL;
}

static PyObject * Cal_writeFile (PyObject * self, PyObject * args) {
    CalStatus status = {.code = OK, .lineto = 0, .linefrom = 0};
    FILE * file;
    char * filename;
    int indexInt;
    PyObject * calToWrite;
    PyObject * compList;
    PyObject * indexObj;
    CalComp * shallow = NULL;

    if (PyTuple_Size(args) == 3 && PyArg_ParseTuple(
      args, "skO", &filename, (unsigned long*)&calToWrite,&compList)) {
        status.lineto = 0;
        status.linefrom = 0;
        if ((file = fopen(filename,"w")) == NULL) {
            fprintf(stderr,"file did not open\n");
            return Py_BuildValue("s","uh oh speghetti-o, write file didnt open");
        }
        if (PyList_Size(compList) == 1) {
            //pyToInt
            indexObj = PyList_GetItem(compList,0);
            indexInt = pyToInt(indexObj);
            status =  writeCalComp(file,((CalComp *)calToWrite)->comp[indexInt]); 
        } else {
            if (PyList_Size(compList) < ((CalComp *)calToWrite)->ncomps) {
                shallow = (CalComp *)calToWrite;
                for (int i = 0; i < shallow->ncomps; i++) {
                    if (checkIn(i,compList) == 0) {
                        shallow->comp[i] = NULL;
                    } 
                } 
                removeNulls(shallow);
                status = writeCalComp(file,shallow);
            } else {
                status =  writeCalComp(file,(CalComp *)calToWrite);
            }
        }
        fclose(file);
        if (status.code == OK) {
            return Py_BuildValue ("i",status.lineto);
        }
    }
    return Py_BuildValue ("i",-1);;
}

static PyObject * Cal_freeFile (PyObject * self, PyObject * args) {
    CalComp * pcal = NULL;

    if (PyTuple_Size(args) == 1 && PyArg_ParseTuple(args, "k", (unsigned long *)&pcal)) {
        freeCalComp(pcal);
        return Py_BuildValue ("s", "OK");
    }
    return NULL;
}

int pyToInt (PyObject * intObj) {
    PyObject * tempList;
    PyObject * newTuple;
    int toReturn;

    tempList = PyList_New(1);
    PyList_SetItem(tempList,0,intObj);
    newTuple = PyList_AsTuple(tempList);
    PyArg_ParseTuple(newTuple,"i",&toReturn);   

    return toReturn; 
}

int checkIn (int index, PyObject * compList) {
    PyObject * listObj;
    int listInt = 0;

    for (int i = 0; i < PyList_Size(compList); i++) {
        listObj = PyList_GetItem(compList,i);
        listInt = pyToInt(listObj);
        if (listInt == index) {
            return 1;       
        }
    }
    return 0;
}

void removeNulls (CalComp * comp) {
    int origSize = comp->ncomps;

    for (int i = 0; i < origSize; i++) {
        for (int n = 0; n < comp->ncomps; n++) {
            if (comp->comp[n] == NULL) {
                for (int k = n; k < comp->ncomps; k++) {
                    if (k != comp->ncomps-1) {
                        comp->comp[k] = comp->comp[k+1];
                    }
                }
                comp->ncomps -= 1;
            }
        }
    }
}

