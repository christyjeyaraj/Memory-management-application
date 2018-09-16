=========================
#   Problem Statement   #
=========================

The goal of this assignment is to create a record manager. The record manager handles tables with a fixed schema.
Clients can insert records, delete records, update records, and scan through the records in a table. A scan is
associated with a search condition and only returns records that match the search condition. Each table should
be stored in a separate page file and record manager should access the pages of the file through the buffer
manager implemented in the last assignment.

===========================
#      Contribution       #
===========================
This is a group project. The group size is 4 And I worked on 
   Helper Functions.
   Tombstone Implementation.
   Memory Leak issues.
   
=========================
#         Logic         #
=========================

Data Structures and Design :
————————————————————————————

We have defined a few attributes that are required by the record manager implemented by a structure of such attributes
called tableInfo. The attributes of this structure are:
a. schemaLength    - length of the schema.
b. recordStartPage - page # of first record in a table.
c. recordLastPage  - page # of last record in a table.
d. numTuples       - defined as # of tuples in a table.
e. slotSize        - defined as size of a slot holding a record.
f. maxSlots        - max # of slots.
g. tstone_root     - tstone_root is the head node of the root.

We have defined a few attributes that are required to perform scan to retrieve all tuples from a table that fulfill a certain
condition into  a structure of such attributes called recordInfo. The attributes are as follows :
a. cond     - which is the condition defined by the client.
b. curSlot  - which is the current slot of the record.
c. curPage  - which is the current page of the record.
d. numPages - the total # of pages.
e. numSlots - the total # of slots.

We have defined a few attributes that are required to track a deleted record which are implemented in a structure containing such attributes
called recordInfo tNode. The attributes are as follows :

id   - it stores page id, slot id and tombstone id of the record.
next - it stores the next node in linked list.

Table and Record Manager Functions :
—————————————————————————————————

initRecordManager:
- initRecordManager starts the record manage and also initializes the set up values. In special case of a primary key constraint, the value 1 is passed as argument.

shutdownRecordManager:
- The shutdownRecordManager simply frees the resources assigned to the record manager.

createTable:

- createTable first creates the table with given name and checks if the table file already exists. If exists, it throws an error message.
or else if a new file created, it writes the tableInfo on the page 0 and schema on page 1. The recordStartPage is set to page 2.

openTable:

- openTable function opens the file with given table name and first checks if the file already exists. If it does not exist then it throws the error and it initializes the buffer manager with given filename.
Further it then reads both the page 0 and page 1, and loads the tableInfo and schema into memory.

closeTable:

- closeTable function closes both the file and the buffermanager of that particular file. If the file does not exist, it throws error.
It then frees all the resources assigned with table.

deleteTable:

- deleteTable function deletes the table file and if the table does not exist, it throws an error. It then deletes the file and destroys the buffer manager associated with it.

getNumTuples:

- As we know, numTuples is stored in memory as part of tableInfo on page 0 when initially written to file and later loaded to memory.
The value is then updated every time a successful insert and delete is called.


Record Functions :
———————————————————————————

insertRecord:

- The insertRecord function initally checks to see if there are any RID's in the tstone list. If any, one of those are used
while the record's attributes are updated accordingly. Otherwise, since empty slots are not available, a new value for slot must be computed. Again, if the new slot's location is equal
to the maximum number of slots for a page, a new page is created or else the current page is used. The record's values for page and slot are updated accordingly. We use serializeRecord to get the record in the proper format.

- The functions pinPage, markDirty, unpinPage, and forcePage are used to update the buffer pool by increasing the number of tuples and the resulting table is written to file.

deleteRecord:

- This function initially checks if the tstone list is empty. If empty, it creates a new tNode and updates
its contents with the values from RID and it is added to the tstone list.
- On the other hand, if list is not empty, then tstone_iter is used to seek to end of the list and then it adds a new tNode
with all the RID's contents there.
- In the last step, the number of tuples are decreased and the result table is written to file.

updateRecord:

- In this function we use serializeRecord to get the record in the proper format.
- The functions pinPage, markDirty, unpinPage, and forcePage are thereafter used to update the buffer pool and the
resulting table is thereafter written to file.

getRecord:

- This function uses the given RID value to return a record to the user.
- It Intially checks whether RID is not in the tstone list. Then, if it is a valid record,
we make another check to see if the tuple number is greater than the number of tuples in the table ( if this is an
error, RC_RM_NO_MORE_TUPLES is returned).
- Then, Buffer pool is updated using pinPage and unpinPage.
- Finally, To obtain a valid record from the record string which was retrieved, deserializeRecord is used. record->data is updated.

Scan Functions:
————————————————————————

startScan:

- startScan initializes the RM_ScanHandle data structure which is passed as an argument to it while also initializing a node which stores the information about the record to be searched and the condition which is to be evaluated. This node is later assigned to scan->mgmtData.

Below steps are executed in order next:
- startScan starts by fetching a record according to the page and slot id.
- Next, a check for tombstone id for a deleted record is placed.
- If the bit is already set and the record is also a deleted one then it a check for the slot number of the record to see if it
is the last record is placed.
- If it is the last record, the slot id is set to be 0 to start next scan from the beginning of the next page.
- If it not the last record, the slot # is incremented by one to proceed to the next record.
- The updated record id parameters are then assigned to the scan mgmtData and next function is called.
- The given condition is then checked to see if the record is same as one requested by the client, after verifying the tombstone parameters of the record.
- If the record fetched is not the required one then the next function with the updated record id parameters is called again.
- It returns RC_RM_NO_MORE_TUPLES once the scan is complete and RC_OK otherwise if no error occurs.


closeScan:

- In closeScan, we set scan handler to be free indicating the record manager that all resources associated are thereby cleaned up.

Schema Functions:
————————————————————————

getRecordSize:

- getRecordSize returns the size of the record, the datatype of each attribute is considered for this calculation first then based on the schema, function counts the size of the record.

createSchema:

-createSchema creates the schema object and assigns the memory.
- The # of attributes, their datatypes and the size is then stored.

freeSchema:

- freeSchema frees all the memory assigned to schema object :
   1. DataType
   2. AttributeSize
   3. AttributeNames

Attribute Functions
————————————————————————

createRecord:

-In createRecord function, memory allocation takes place for record and its corresponding record data for creating a new record. Memory allocation then happens as per schema.

freeRecord:
freeRecord frees the memory space allocated to record and its data.

getAttr:
- It starts by allocating the space to the value data structure where attribute values are to be fetched.
- Next, as per the attribute numbers, attrOffset function is called to get the offset value of different attributes.
- Lastly, as per different data types, the attribute data is assigned to the value pointer, thereby fetching the attribute values of a record.

setAttr:
- Initially starts by calling the attrOffset function to get the offset value of different attributes.
- Secondly, attribute values are set with the values provided by the client thereby setting the attribute values of a record.


Helper Functions:
————————————————————————

tableInfo :

- tableInfo function writes the tablenInfo to the file. On page 0, the tableInfo is written. The keyinfo is written on the page 2.

deserializeRecord :

- deserializeRecord reads the record from the table file, parses it, and returns a record data object.

deserializeSchema :

- deserializeSchema reads the schema from the table file and then parses it and returns a schema object.

slotSize :

- Based on the serializeRecord function, slotSize calculates the slot size required to write one record on the file .

tableInfo and strToTable :

- These 2 functions convert the tableInfo to a string to write on the file and then converts the read data from the file
to tableInfo object.

keyInfoToStr and strToKeyInfo :

- These 2 functions converts the keyAttributeInfo to a string to write on the file and then converts the read data from the file to keyList.


==========================
#   Additional Features  #
==========================

Tombstones :
—————————————

- Tombstones are stored in a linked list tableInfo->tstone_root.
- The list consists of tNodes where each tNode contains an RID and a pointer to the next tNode. This list is used in the record functions.
- Also, the RID has another attribute viz., tstone (boolean), which is true if that RID is a tombstone which helps the scan functions to easily check to see which values need to be skipped when traversing records. The RID struct was also altered in tables.h.

Additional Error checks :
————————————————————————
Below error cases are checked and tested :

a. Create a table which already exists.
b. Close a table which is not open.
c. Open or close a table which does not exist.
d. delete or update a record which does not exist.

Memory leaks :
—————————————————

No Memory leaks
