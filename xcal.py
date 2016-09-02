#!/usr/bin/python3

#xcal.py - gui used to display and manipulate iCal files
#Last Modified: 330pm April 6, 2016
#Geofferson Camp (gcamp@mail.uoguelph.ca)
#0658817
#
# - Added database functionality for A4

import string
import sys
import CalModule
import subprocess
import getpass
import datetime
import mysql.connector
from tkinter import *
import tkinter.ttk as ttk
import tkinter.tix as Tix
from tkinter.scrolledtext import *
from tkinter.filedialog import *
from tkinter.messagebox import * 

class view(Frame):
    def __init__(self,master):
        Frame.__init__(self,master)
        self.username = ""
        self.password = ""
        self.connect = None
        self.cursor = None
        self.dbSetUp()
        self.master.title("xcal")
        root.resizable(True,True)
        self.calFile = None
        self.inFVP = list()
        self.todoSel = dict()
        self.undoes = list()
        self.activeICS = ""
        self.unsaved = 0
        self.prevPrint = 0
        self.fileFrame = FilePanel(self)
        self.logFrame = LogPanel(self)
        self.menus() 
        root.grid_columnconfigure(1,weight=1)
        root.grid_rowconfigure(1,weight=4)
        root.grid_rowconfigure(2,weight=1)
        root.minsize(600,500)
        self.checkDatemask()
        #shortcuts
        root.protocol('WM_DELETE_WINDOW',self.exit)
        self.bind_all("<Control-x>",lambda _:self.exit())
        self.bind_all("<Control-o>",lambda _:self.open())
        self.bind_all("<Control-s>",lambda _:self.save())
        self.bind_all("<Control-t>",lambda _:self.todoWin(self.calFile)) 
        self.bind_all("<Control-z>",lambda _:self.undo())

    def makeTables(self):
        sql = """CREATE TABLE IF NOT EXISTS ORGANIZER (
                 org_id INT AUTO_INCREMENT PRIMARY KEY,
                 name VARCHAR (60) NOT NULL,
                 contact VARCHAR (60) NOT NULL)"""
        self.cursor.execute(sql)
        sql = """CREATE TABLE IF NOT EXISTS EVENT (
                 event_id INT AUTO_INCREMENT PRIMARY KEY,
                 summary VARCHAR (60) NOT NULL,
                 start_time DATETIME NOT NULL,
                 location VARCHAR(60),
                 organizer INT,
                 FOREIGN KEY (organizer) REFERENCES ORGANIZER (org_id))"""
        self.cursor.execute(sql)
        sql = """CREATE TABLE IF NOT EXISTS TODO (
                 todo_id INT AUTO_INCREMENT PRIMARY KEY,
                 summary VARCHAR(60) NOT NULL,
                 PRIORITY SMALLINT,
                 organizer INT,
                 FOREIGN KEY (organizer) REFERENCES ORGANIZER (org_id))"""
        self.cursor.execute(sql)

    def dbSetUp(self):
        if (len(sys.argv) > 1):
            self.username = sys.argv[1]
            tries = 0
            connected = 0
            conObj = mysql.connector
            while (tries < 3 and connected == 0):
                self.password = getpass.getpass()
                try:
                    self.connect = conObj.connect(user=self.username,password=self.password,
                    host='dursley.socs.uoguelph.ca',database=self.username)
                    connected = 1
                except conObj.Error as err:
                    connected = 0
                    print("Database connection failed.")
                    print(err)  
                    tries = tries + 1
            if (tries > 2):
                #self.connect.close() 
                sys.exit()
            self.cursor = self.connect.cursor()
            self.makeTables()
        else:
            print("database username paramter missing")
            sys.exit()
            
    def menus(self):
        menuBar = Menu(root)
        root.config(menu=menuBar)

        self.fileMenu = Menu(menuBar,tearoff=0) 
        self.fileMenu.add_command(label="Open...", command=self.open)
        self.fileMenu.add_command(state=DISABLED, label="Save", command=self.save)
        self.fileMenu.add_command(state=DISABLED, label="Save as", command=self.saveAs)
        self.fileMenu.add_command(state=DISABLED, label="Combine...", command=self.combine)
        self.fileMenu.add_command(state=DISABLED, label="Filter...", command=self.filter)
        self.fileMenu.add_separator()
        self.fileMenu.add_command(label="Exit", command=self.exit) 
        menuBar.add_cascade(label="File",menu=self.fileMenu)

        self.todoMenu = Menu(menuBar,tearoff=0)
        self.todoMenu.add_command(state=DISABLED, label="To-do List...", 
        command=lambda:self.todoWin(self.calFile))
        self.todoMenu.add_command(state=DISABLED, label="Undo...", command=self.undo)
        menuBar.add_cascade(label="Todo",menu=self.todoMenu)

        self.dbMenu = Menu(menuBar,tearoff=0)
        self.dbMenu.add_command(state=DISABLED, label="Store All", command=self.storeAll)
        self.dbMenu.add_command(state=DISABLED, label="Store Selected", command=self.storeSel)
        self.dbMenu.add_command(state=DISABLED, label="Clear", command=self.clearDB)
        self.dbMenu.add_command(label="Status", command=self.printStatus)
        self.dbMenu.add_command(label="Query...", command=self.openQuery)
        menuBar.add_cascade(label="Database",menu=self.dbMenu)       

        helpMenu = Menu(menuBar,tearoff=0)
        helpMenu.add_command(label="Date Mask...", command=self.dateMask)
        helpMenu.add_command(label="About xcal...", command=self.about)
        menuBar.add_cascade(label="Help",menu=helpMenu) 

        counts = self.countDB()
        if (counts[0] > 0 or counts[1] > 0 or counts[2] > 0):
            self.activateClear()
            
    def clearDB(self):
        clearTodoQ = """DELETE FROM TODO"""
        self.cursor.execute(clearTodoQ)
        clearEvQ = """DELETE FROM EVENT"""
        self.cursor.execute(clearEvQ)
        clearOrgQ = """DELETE FROM ORGANIZER"""
        self.cursor.execute(clearOrgQ)
        self.connect.commit()
        self.dbMenu.entryconfig("Clear",state="disabled")
        self.printStatus()

    def countDB(self):
        counts = list()
        countOrgQ = """SELECT org_id FROM ORGANIZER"""
        self.cursor.execute(countOrgQ)
        result = self.cursor.fetchall()
        counts.append(len(result))
        countEvQ = """SELECT event_id FROM EVENT"""
        self.cursor.execute(countEvQ)
        result = self.cursor.fetchall()
        counts.append(len(result))
        countTodoQ = """SELECT todo_id FROM TODO"""
        self.cursor.execute(countTodoQ)
        result = self.cursor.fetchall()
        counts.append(len(result))
        return counts
      
    def printStatus(self):
        counts = self.countDB()
        self.logFrame.log.insert(INSERT,
        "Database has "+str(counts[0])+" organizer(s), "+str(counts[1])+" event(s), "+str(counts[2])+" to-do item(s).\n")
        self.logFrame.log.see(END)

    def insertOrg(self,name,contact): 
        insertQ = """INSERT INTO ORGANIZER (name,contact) \
                   VALUES (%s,%s)"""
        self.cursor.execute(insertQ,(name,contact))
        self.connect.commit()

    def addEvent(self,index,orgId):
        summary = self.calFile[index+3]
        if (summary == ""):
            return
        dateToParse = self.calFile[index+6]
        if (dateToParse == ""):
            date = datetime.datetime(2016,5,8)
        else:
            date = datetime.datetime(int(dateToParse[:4]),int(dateToParse[4:6]),
            int(dateToParse[6:8]),int(dateToParse[9:11]),
            int(dateToParse[11:13]),int(dateToParse[13:15]))

        location = self.calFile[index+7]
        checkQ = """SELECT event_id FROM EVENT WHERE \
                    summary = %s AND start_time = %s"""
        self.cursor.execute(checkQ,(summary,date))
        result = self.cursor.fetchall()
        if (len(result) != 0):
            return
        eventQ = """INSERT INTO EVENT (summary,start_time,location,organizer) \
                    VALUES (%s,%s,%s,%s)"""
        self.cursor.execute(eventQ,(summary,date,location,orgId))
        self.connect.commit()


    def addTodo(self,index,orgId):
        summary = self.calFile[index+3]
        if (summary == ""):
            return
        priority = self.calFile[index+6]
        checkQ = """SELECT todo_id FROM TODO WHERE \
                    summary = %s"""
        self.cursor.execute(checkQ,(summary,))
        result = self.cursor.fetchall()
        if (len(result) != 0):
            return
        eventQ = """INSERT INTO TODO (summary,priority,organizer) \
                    VALUES (%s,%s,%s)"""
        self.cursor.execute(eventQ,(summary,priority,orgId))
        self.connect.commit()

    def storeOne(self,index):
        orgId = None
        if (self.calFile[index+4] != ""):
            orgName = self.calFile[index+4]
            orgContact = self.calFile[index+5]
            orgQuery = """SELECT org_id \
                        FROM ORGANIZER \
                        WHERE name = %s  AND contact = %s"""
            self.cursor.execute(orgQuery,(orgName,orgContact))
            result = self.cursor.fetchall()
            if (len(result) == 0):
                self.insertOrg(orgName,orgContact);
            self.cursor.execute(orgQuery,(orgName,orgContact))
            result = self.cursor.fetchall()
            orgId = result[0][0]
        if (self.calFile[index] == "VEVENT"): 
            self.addEvent(index,orgId)
            self.activateClear()
        elif (self.calFile[index] == "VTODO"):
            self.addTodo(index,orgId) 
            self.activateClear()

    def storeSel(self):
        curItem = self.fileFrame.tree.focus()
        #compRef = self.fileFrame.tree.item(curItem)['values'][0]-1
        self.storeOne(int(curItem))
        self.printStatus()

    def storeAll(self):
        for index in range(1,len(self.calFile),8):
            self.storeOne(index)
        self.printStatus()

    def nameFromPath(self,path):
        counter = 0
        slashes = list()
        for letter in path:
            if (letter == '/'):
                slashes.append(counter)
            counter = counter + 1
        if (len(slashes) == 0):
            return path
        return path[-(counter-slashes[len(slashes)-1]-1):]
 
    def checkDatemask(self):
        var = os.environ.get("DATEMSK")
        if (var == None):
            setCheck = askokcancel(title="Set Date Mask",
            message="Do you want to set the date mask variable now?")
            if (setCheck == True):
                self.dateMask()

    #file menu
    def open(self):
        if (self.unsaved == 1):
            overWrite = askokcancel(title="Confirm Action",message="Discard iCal changes?")
            if (overWrite == False):
                return
        fd = LoadFileDialog(self,"Open iCal File")
        fileName = fd.go()
        if (fileName == None):
            return
        os.system("./caltool -info < " + fileName + " > tempOut 2> tempErr")
        infoCheck = self.checkTemps(1,fileName)
        if (infoCheck == 2):
            return
        self.fileFrame.showSelBtn.config(state="disabled") 
        self.master.title(self.nameFromPath(fileName));
        self.activeICS = fileName
        self.activateBtns()
        self.readNewCal(fileName)
        self.unsaved = 0

    def readNewCal (self,fileName):
        self.inFVP.clear()      
        result = []
        status = CalModule.readFile(fileName,result); 
        compNum = 1
        self.fileFrame.tree.delete(*self.fileFrame.tree.get_children())
        for index in range(1,len(result),8):
            self.fileFrame.tree.insert('','end',iid=index,
            values=(compNum,result[index],result[index+1],result[index+2],result[index+3]))
            self.inFVP.append(compNum-1)
            compNum += 1
        if (self.calFile != None):
            CalModule.freeFile(self.calFile[0]);        
        self.calFile = result
        lines = CalModule.writeFile("tempCal",self.calFile[0],self.inFVP)        
        self.correctLines(lines) 

    def checkTemps(self,checkOut,fileName):
        if (checkOut == 1):    
            tempOut = open("tempOut","r")
            outStr = tempOut.read()
            self.logFrame.log.insert(INSERT,outStr)
            self.logFrame.log.see(END)
        tempErr = open("tempErr","r")
        errStr = tempErr.read()
        if (errStr == ""):
            return 1
        else:
            if (fileName != None):
                errStr = errStr.rstrip()
                errStr = errStr + " for file " + self.nameFromPath(fileName) + "\n"
            self.logFrame.log.insert(INSERT,errStr)
            self.logFrame.log.see(END)
            return 2   

    def correctLines(self,lines):
        lines =  lines - self.prevPrint
        self.prevPrint = self.prevPrint + lines
        return lines 

    def save(self):
        if (self.calFile == None):
            return
        lines = CalModule.writeFile(self.activeICS,self.calFile[0],self.inFVP)
        lines = self.correctLines(lines)
        if (lines == -1):
            lines = 0
            report = "Save failed."
        else:
            report = "Save successful."
        self.logFrame.log.insert(INSERT,report+" Wrote "+str(lines)+" lines\n")
        self.logFrame.log.see(END) 
        self.unsaved = 0
        self.master.title(self.nameFromPath(self.activeICS))
    def saveAs(self):
        fd = LoadFileDialog(self,"Save as File Selection")
        fileName = fd.go()
        self.activeICS = fileName
        self.save()
    def combine(self):
        fd = LoadFileDialog(self,"Combine File Selection")
        fileName = fd.go()
        if (fileName == None):
            return
        os.system("./caltool -combine "+fileName+" < tempCal > tempOut 2> tempErr")
        check = self.checkTemps(0,fileName)
        if (check == 1):
            self.readNewCal("tempOut")
            self.changes()
    def filter(self):
        def subFilter():
            if (controlVar.get() == 1):
                filterFlag = "t"
                timeString =""
            else:
                filterFlag = "e"
                fromTime = fromBox.get("1.0",END).rstrip()
                toTime = toBox.get("1.0",END).rstrip()
                if (fromTime != "" or toTime != ""):
                    timeString = " "
                    if (fromTime != ""):
                        timeString = timeString + "from \""+fromTime+"\""
                    if (toTime != ""):
                        if (fromTime != ""):
                            timeString = timeString + " "
                        timeString = timeString + "to \""+toTime+"\""
            os.system("./caltool -filter "+filterFlag+timeString+" < tempCal > tempOut 2> tempErr")
            check = self.checkTemps(0,None)
            if (check == 1):
                self.readNewCal("tempOut")
                self.changes()
            filterWin.destroy()
        def cancel():
            filterWin.destroy()
        def filterStatus():
            filterBtn.config(state="normal")
                
        filterWin = Toplevel()
        filterWin.title("Filter")
        filterWin.minsize(100,100)
        controlVar = IntVar()
        todoRadio = Radiobutton(filterWin,variable=controlVar,
        value=1,text="Todo Items",command=filterStatus)
        todoRadio.grid(row=1,column=1)
        eventsRadio = Radiobutton(filterWin,variable=controlVar,
        value=2,text="Event Items",command=filterStatus)
        eventsRadio.grid(row=2,column=1)
        fromLabel = Label(filterWin,text="from:")
        fromLabel.grid(row=2,column=2)
        fromBox = Text(filterWin,height=1,width=10)
        fromBox.grid(row=2,column=3)
        toLabel = Label(filterWin,text="to:")
        toLabel.grid(row=2,column=4)
        toBox = Text(filterWin,height=1,width=10)
        toBox.grid(row=2,column=5)
        cancelBtn = Button(filterWin,text="Cancel",command=cancel)
        filterBtn = Button(filterWin,text="Filter",command=subFilter,state=DISABLED)
        filterBtn.grid(row=3,column=1)
        cancelBtn.grid(row=3,column=2)
        filterWin.bind("<Escape>",lambda _:cancel())
        filterWin.focus_force() 
        
    def exit(self):
        leave = askokcancel(title="Confirm Exit",
        message="Close program? All unsaved changes will be lost.")
        if (leave == False):
            return
        if (self.calFile != None):
            CalModule.freeFile(self.calFile[0]);
        root.destroy()
        self.connect.close()
        if (os.path.isfile("tempErr")):
            os.remove("tempErr")
        if (os.path.isfile("tempCal")):
            os.remove("tempCal")
        if (os.path.isfile("tempOut")):
            os.remove("tempOut")
        if (os.path.isfile("tempUndo")):
            os.remove("tempUndo")

    def changes(self):
        self.master.title(self.nameFromPath(self.activeICS+"*"))
        self.unsaved = 1

    #query window
    def openQuery(self):
        self.queryWindow = Toplevel()
        self.queryWindow.minsize(400,400)
        self.queryWindow.title("Query")
        self.queryWindow.grid_columnconfigure(1,weight=2)
        self.queryWindow.grid_columnconfigure(2,weight=1)
        self.dbMenu.entryconfig("Query...",state="disabled")
        self.queryWindow.protocol('WM_DELETE_WINDOW',self.exitQuery)

        self.queryWindow.query = Frame(self.queryWindow,relief=RAISED)
        self.queryWindow.query.grid(row=1,column=1,sticky=NSEW)
        controlVar = IntVar()
        controlVar.set(-1)
        oneRadio = Radiobutton(self.queryWindow.query,variable=controlVar,
        value=1,text="Display the items of organizer ______ (SQL wild card % permitted).")
        oneRadio.grid(row=2,column=1,columnspan=5)
        twoRadio = Radiobutton(self.queryWindow.query,variable=controlVar,
        value=2,text="How many events take place in _______ (location)?")
        twoRadio.grid(row=3,column=1,columnspan=5)
        threeRadio = Radiobutton(self.queryWindow.query,variable=controlVar,
        value=3,text="Describe all events starting after ______.")
        threeRadio.grid(row=4,column=1,columnspan=5)
        fourRadio = Radiobutton(self.queryWindow.query,variable=controlVar,
        value=4,text="Summarize todos with prority ______.")
        fourRadio.grid(row=5,column=1,columnspan=5)
        fiveRadio = Radiobutton(self.queryWindow.query,variable=controlVar,
        value=5,text="Summarize todos with the highest priority.")
        fiveRadio.grid(row=6,column=1,columnspan=5)
        optionBox = Text(self.queryWindow.query,height=1)#width=100
        optionBox.grid(row=7,column=1,columnspan=5)
        optionBox.insert(INSERT,"(fill in blank)")
        sixRadio = Radiobutton(self.queryWindow.query,variable=controlVar,
        value=-1,text="Custom")
        sixRadio.grid(row=8,column=1,columnspan=5)

        customBox = Text(self.queryWindow.query,height=1)#width=100
        customBox.grid(row=9,column=1,columnspan=5)
        customBox.insert(INSERT,"SELECT")

        def doQuery():
            option = optionBox.get("1.0",END)
            option = option.rstrip('\n')
            var = controlVar.get()
            if (var == -1):
                sql = customBox.get("1.0", END)
            elif (var == 1):
                orgSql = """SELECT org_id FROM ORGANIZER WHERE name LIKE %s"""
                self.cursor.execute(orgSql,(option,))
                result = self.cursor.fetchall()
                if (len(result) == 0):
                    self.queryWindow.results.log.insert(INSERT, "No results found.\n------------------\n")
                    self.queryWindow.results.log.see(END)
                    return
                option2 = result[0][0]
                option3 = option2
                sql = """SELECT summary \
                         FROM EVENT \
                         WHERE organizer = %s \
                         UNION \
                         SELECT summary \
                         FROM TODO \
                         WHERE organizer = %s"""
            elif (var == 2):
                sql = """SELECT COUNT(*) FROM EVENT WHERE location = %s"""
            elif (var == 3):
                match = re.match('\d\d\d\d \d\d \d\d',option,flags=0)
                if (match):
                    match = "hi"
                else:
                    self.queryWindow.results.log.insert(INSERT, "Enter date in the form: 'YYYY MM DD'\n----------------\n")
                    self.queryWindow.results.log.see(END)
                    return
                option = datetime.datetime(int(option[:4]),int(option[5:7]),int(option[8:]))
                sql = """SELECT summary FROM EVENT WHERE start_time >= %s"""
            elif (var == 4):
                sql = """SELECT summary from TODO WHERE priority = %s"""
            elif (var == 5):
                sql = """SELECT summary FROM TODO WHERE priority IN (\
                         SELECT MIN(priority) FROM TODO WHERE priority > 0)"""
            try:
                if (var == -1 or var == 5):
                    self.cursor.execute(sql)
                elif (var == 1):
                    self.cursor.execute(sql,(option2,option3))
                elif (var == 2 or var == 3 or var == 4):
                    self.cursor.execute(sql,(option,))
            except mysql.connector.Error as e:
                self.queryWindow.results.log.insert(INSERT, str(e)+"\n-----------------\n")
                return
            result = self.cursor.fetchall()
            #printOrg(result)
            resultStr = str(result)
            resultStr = resultStr.replace("), (","\n")
            resultStr = resultStr.replace("[(","")
            resultStr = resultStr.replace(")]","")
            resultStr = resultStr+"\n"
            self.queryWindow.results.log.insert(INSERT, resultStr+"------------------\n")
            self.queryWindow.results.log.see(END)

        #cancelBtn = Button(filterWin,text="Cancel",command=cancel)
        submitBtn = Button(self.queryWindow.query,text="Submit",command=doQuery)
        submitBtn.grid(row=10,column=1,columnspan=5)

        def openHelp():
            helpWindow = Toplevel()
            helpWindow.minsize(400,400)
            helpWindow.title("Help")

            sql = """DESCRIBE ORGANIZER"""
            self.cursor.execute(sql)
            result = self.cursor.fetchall()

            resultStr = str(result)
            resultStr = resultStr.replace("), (","\n")
            resultStr = resultStr.replace("[(","")
            resultStr = resultStr.replace(")]","")
            resultStr = "ORGANIZER\n"+resultStr
            orgLabel = Label(helpWindow,text=resultStr)
            orgLabel.grid(row=1,column=1)

            sql = """DESCRIBE EVENT"""
            self.cursor.execute(sql)
            result = self.cursor.fetchall()

            resultStr = str(result)
            resultStr = resultStr.replace("), (","\n")
            resultStr = resultStr.replace("[(","")
            resultStr = resultStr.replace(")]","")
            resultStr = "EVENT\n"+resultStr
            eventLabel = Label(helpWindow,text=resultStr)
            eventLabel.grid(row=2,column=1)

            sql = """DESCRIBE TODO"""
            self.cursor.execute(sql)
            result = self.cursor.fetchall()

            resultStr = str(result)
            resultStr = resultStr.replace("), (","\n")
            resultStr = resultStr.replace("[(","")
            resultStr = resultStr.replace(")]","")
            resultStr = "TODO\n"+resultStr
            todoLabel = Label(helpWindow,text=resultStr)
            todoLabel.grid(row=3,column=1)

        helpBtn = Button(self.queryWindow.query,text="Help",command=openHelp)
        helpBtn.grid(row=11,column=1,columnspan=5)

        #log panel
        def clear():
            self.queryWindow.results.log.delete(1.0, END)

        self.queryWindow.results = Frame(self.queryWindow,relief=RAISED)
        self.queryWindow.results.grid(row=1,column=2,sticky=NSEW)
        #self.queryWindow.results.grid_columnconfigure(1,weight=1)
        self.queryWindow.results.log = ScrolledText(self.queryWindow.results)
        self.queryWindow.results.log.grid(row=1,column=1,sticky=NSEW)
        clearBtn = Button(self.queryWindow.results,text="Clear",
        command=lambda:clear()).grid(row=2,column=1) 

    #todo menu
    def todoWin (self,calFile):
        if (self.calFile == None):
            return       
        def cancel():
            self.todoWindow.destroy()    

        self.todoWindow = Toplevel()
        canvas = Canvas(self.todoWindow)
        frame = Frame(canvas)
        confirm = Button(self.todoWindow,text="Done",state=DISABLED,command=self.updateFromTodo)

        def activeDone():
            confirm.config(state="normal")

        def setConfig(event):
            canvas.configure(scrollregion=canvas.bbox('all'),width=250,height=200) 
        
        canvas.pack(side="left",fill="both",expand=True) 
        self.vsb = Scrollbar(self.todoWindow, orient="vertical",command=canvas.yview)
        self.vsb.pack(side=RIGHT,fill=Y)
        canvas.configure(yscrollcommand=self.vsb.set)
        self.todoWindow.title("To-do List")
        self.todoSel.clear()
        rowCount = 1
        for index in range(1,len(calFile),8):
            if (calFile[index] == "VTODO"):
                self.todoSel[index]=IntVar()
                c = Checkbutton(frame,text=calFile[index+3],
                variable=self.todoSel[index],command=activeDone) 
                c.pack() 

        #add cancel and confirm buttons
        confirm.pack(padx=(0,10),pady=(10,10))
        cancelBtn = Button(self.todoWindow,text="Cancel", command=cancel)
        cancelBtn.pack(padx=(0,10))
        self.todoWindow.bind("<Escape>",lambda _:cancel())
        self.todoWindow.focus_force()
        canvas.create_window((0,0),window=frame,anchor='nw')
        frame.bind("<Configure>",setConfig) 

    def updateFromTodo(self):
        lines = CalModule.writeFile("tempUndo",self.calFile[0],self.inFVP)
        self.correctLines(lines)
        for index in self.todoSel:
            if (self.todoSel[index].get() == 1):
                self.undoes.append((int(index/4)+1,index))
                self.inFVP.remove(int(index/4))
        self.todoSel.clear()
        lines = CalModule.writeFile("tempCal",self.calFile[0],self.inFVP)
        self.correctLines(lines)
        self.readNewCal("tempCal")
        self.changes()
        self.todoMenu.entryconfig("Undo...",state="normal")
        self.todoWindow.destroy()

    def undo(self):
        if (len(self.undoes) == 0):
            return
        undoC = askokcancel(title="Confirm Undo",
        message="Restore all removed components since last save?")
        if (undoC == False):
            return
        self.readNewCal("tempUndo")
        self.todoMenu.entryconfig("Undo...",state="disabled")
        self.undoes.clear()    

    #help menu
    def dateMask(self):
        fd = LoadFileDialog(self,"Date mask template selection")
        fileName = fd.go()
        if (fileName == None):
            return
        os.environ["DATEMSK"] = fileName
    def about(self):
        aboutWin = Toplevel()
        aboutWin.title("About")
        aboutWin.minsize(300,80)
        title = Label(aboutWin, text="xCal Appliation")
        title.pack()
        author = Label(aboutWin, text="Geofferson Camp")
        author.pack()
        misc = Label(aboutWin, text="Compatible with iCalendar V2.0")
        misc.pack()

    def activateBtns(self):
        self.fileFrame.extractEvBtn.config(state="normal")
        self.fileFrame.extractXBtn.config(state="normal")
        self.fileMenu.entryconfig("Save",state="normal")
        self.fileMenu.entryconfig("Save as",state="normal")
        self.fileMenu.entryconfig("Combine...",state="normal")
        self.fileMenu.entryconfig("Filter...",state="normal")
        self.todoMenu.entryconfig("To-do List...",state="normal")
        self.dbMenu.entryconfig("Store All",state="normal")
        
    def activateSelect(self,event):
        if (self.calFile != None and self.fileFrame.tree.focus() != ""):
            self.fileFrame.showSelBtn.config(state="normal")
            self.dbMenu.entryconfig("Store Selected",state="normal")

    def activateClear(self):
        self.dbMenu.entryconfig("Clear",state="normal")

    def exitQuery(self):
        self.dbMenu.entryconfig("Query...",state="normal")
        self.queryWindow.destroy()

class FilePanel(Frame):
    def __init__(self,master):
        Frame.__init__(self,master)

        #file panel functions
        def showSel():
            curItem = self.tree.focus()
            compRef = self.tree.item(curItem)['values'][0]-1
            lines = CalModule.writeFile("tempOut",master.calFile[0],[compRef])
            master.correctLines(lines)
            master.checkTemps(1,None)
        def extractEv():
            os.system("./caltool -extract e < tempCal > tempOut 2> tempErr")
            master.checkTemps(1,None)
        def extractX():
            os.system("./caltool -extract x < tempCal > tempOut 2> tempErr")
            master.checkTemps(1,None)

        self.fileFrame = Frame(root,relief=RAISED)
        self.fileFrame.grid(row=1,column=1,sticky=NSEW)
        self.fileFrame.grid_columnconfigure(1,weight=1)
        self.fileFrame.grid_columnconfigure(2,weight=1)
        self.fileFrame.grid_columnconfigure(3,weight=1)
        self.fileFrame.grid_rowconfigure(1,weight=1)
        self.fileFrame.grid_rowconfigure(2,weight=0)
        self.tree = ttk.Treeview(self.fileFrame,
        selectmode="browse",columns=('No.',"Name","Props","Subs","Summary"))
        self.tree.bind("<ButtonRelease-1>", master.activateSelect)
        self.tree.grid(row=1,column=1,columnspan=3,sticky=NSEW)
        self.tree.column('#0', width=0, minwidth=0, stretch=NO)
        self.tree.column('#1',width=50)
        self.tree.column('#3',width=50)
        self.tree.column('#4',width=50)
        self.tree.heading('No.',text="No.")
        self.tree.heading('Name',text="Name")
        self.tree.heading('Props',text="Props")
        self.tree.heading('Subs',text="Subs")
        self.tree.heading('Summary',text="Summary")
        self.showSelBtn = Button(self.fileFrame,text="Show Selected", 
        command=lambda:showSel(),state=DISABLED)
        self.showSelBtn.grid(row=2,column=1)
        self.extractEvBtn = Button(self.fileFrame,text="Extract Events", 
        command=extractEv,state=DISABLED)
        self.extractEvBtn.grid(row=2,column=2)
        self.extractXBtn = Button(self.fileFrame,text="Extract X-Props", 
        command=extractX,state=DISABLED)
        self.extractXBtn.grid(row=2,column=3) 

class LogPanel(Frame): 
    def __init__(self,master):
        Frame.__init__(self,master)

        #log panel
        def clear():
            self.log.delete(1.0, END)

        self.logFrame = Frame(root,relief=RAISED)
        self.logFrame.grid(row=2,column=1,sticky=EW)
        self.logFrame.grid_columnconfigure(1,weight=1)
        self.logFrame.grid_columnconfigure(2,weight=0)
        self.log = ScrolledText(self.logFrame,height=10)
        self.log.grid(row=1,column=1,sticky=NSEW)
        #self.log.insert(INSERT, "sup bois")
        clearBtn = Button(self.logFrame,text="Clear",command=lambda:clear()).grid(row=1,column=2) 

root = Tk()

view = view(root)
root.mainloop()


