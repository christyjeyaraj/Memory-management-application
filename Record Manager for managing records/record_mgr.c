#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include "record_mgr.h"

/* Copied from the rm_serializer file for extra serialization.*/

typedef struct VarString {
    char *buf;
    int size;
    int bufsize;
} VarString;

//static Schema Globalschema;
#define MAKE_VARSTRING(var)				\
do {							\
var = (VarString *) malloc(sizeof(VarString));	\
var->size = 0;					\
var->bufsize = 100;					\
var->buf = malloc(100);				\
} while (0)

#define FREE_VARSTRING(var)			\
do {						\
free(var->buf);				\
free(var);					\
} while (0)

#define GET_STRING(result, var)			\
do {						\
result = malloc((var->size) + 1);		\
memcpy(result, var->buf, var->size);	\
result[var->size] = '\0';			\
} while (0)

#define RETURN_STRING(var)			\
do {						\
char *resultStr;				\
GET_STRING(resultStr, var);			\
FREE_VARSTRING(var);			\
return resultStr;				\
} while (0)

#define ENSURE_SIZE(var,newsize)				\
do {								\
if (var->bufsize < newsize)					\
{								\
int newbufsize = var->bufsize;				\
while((newbufsize *= 2) < newsize);			\
var->buf = realloc(var->buf, newbufsize);			\
}								\
} while (0)

#define APPEND_STRING(var,string)					\
do {									\
ENSURE_SIZE(var, var->size + strlen(string));			\
memcpy(var->buf + var->size, string, strlen(string));		\
var->size += strlen(string);					\
} while(0)

#define APPEND(var, ...)			\
do {						\
char *tmp = malloc(10000);			\
sprintf(tmp, __VA_ARGS__);			\
APPEND_STRING(var,tmp);			\
free(tmp);					\
} while(0)

/* copied from rm_serializer file for attribute functions */
RC
attrOffset (Schema *schema, int attrNum, int *result)
{
    int offset = 0;
    int attrPos = 0;

    for(attrPos = 0; attrPos < attrNum; attrPos++)
        switch (schema->dataTypes[attrPos])
    {
        case DT_STRING:
            offset += schema->typeLength[attrPos];
            break;
        case DT_INT:
            offset += sizeof(int);
            break;
        case DT_FLOAT:
            offset += sizeof(float);
            break;
        case DT_BOOL:
            offset += sizeof(bool);
            break;
    }

    *result = offset;
    return RC_OK;
}


/* Used in scan functions*/
typedef struct recordInfo {
    Expr *cond;
    int curSlot;
    int curPage;
    int numPages;
    int numSlots;

}recordInfo;

/* list of tombstones */
typedef struct tNode {
    RID id;
    struct tNode *next;
}tNode;

/* record_mgr starts. */

typedef struct tableInfo{

    int schemaLength;
    int recordStartPage;
    int recordLastPage;
    int numTuples;
    int slotSize;
    int maxSlots;
    int tNodeLen;
    tNode *tstone_root;
    BM_BufferPool *bm;
}tableInfo;


int slotSize(Schema *schema)
{
	int flag=1;
    int size=0, j=0, tempSize;
	while(flag)
	{
		size = size + (1 + 5 + 1 + 5 + 1 + 1 + 1); // 2 square brackets, 2 int, 1 hyphen, 1 space, 1 bracket.

		while(j<schema->numAttr)
		{
			if(schema->dataTypes[j] == DT_STRING)
			{
				tempSize = schema->typeLength[j];
			}
			else if(schema->dataTypes[j] == DT_INT)
			{
				tempSize = 5;
			}
			else if(schema->dataTypes[j] == DT_FLOAT)
			{
				tempSize = 10;
			}
			else if(schema->dataTypes[j] == DT_BOOL)
			{
				tempSize = 5;
			}
			size = size + (tempSize + strlen(schema->attrNames[j]) + 1 + 1);     // attrname_len, dataType_length, colon, coma or ending bracket.
			++j;
		}
		flag=0;
	}
    return size;
}

char *tableToStr(tableInfo *info)
{
	int flag=1;
	while(flag)
	{
		VarString *res;
		MAKE_VARSTRING(res);
		int tnode_len;
		int length = info->schemaLength;
		int start = info->recordStartPage;
		int last = info->recordLastPage;
		int tuples = info->numTuples;
		int slot = info->slotSize;
		int maxs = info->maxSlots;
		APPEND(res, "SchemaLength <%i> FirstRecordPage <%i> LastRecordPage <%i> NumTuples <%i> SlotSize <%i> MaxSlots <%i> ", length, start, last, tuples, slot, maxs);
		tNode *temp_root;
		temp_root = info->tstone_root;

		for(tnode_len=0;temp_root != NULL;tnode_len++)
		{
			temp_root = temp_root->next;
		}
		APPEND(res, "tNodeLen <%i> <", tnode_len);
		temp_root = info->tstone_root;
		while(temp_root != NULL)
		{
			APPEND(res,"%i:%i%s ",temp_root->id.page, temp_root->id.slot, (temp_root->next != NULL) ? ", ": "");
			temp_root = temp_root->next;
		}
		APPEND_STRING(res, ">\n");
		char *resultStr;
		GET_STRING(resultStr, res);
		return resultStr;
		flag=0;
	}

}

tableInfo *strToTable(char *info_str)
{
	int flag = 1;
	if(flag)
	{
		tableInfo *info = (tableInfo*) malloc(sizeof(tableInfo));

		char info_data[strlen(info_str)];
		strcpy(info_data, info_str);

		char *tmp1, *tmp2;
		tmp1 = strtok (info_data,"<");
		tmp1 = strtok (NULL,">");
		int length = strtol(tmp1, &tmp2, 10);
		info->schemaLength = length;

		tmp1 = strtok (NULL,"<");
		tmp1 = strtok (NULL,">");
		int start = strtol(tmp1, &tmp2, 10);
		info->recordStartPage = start;

		tmp1 = strtok (NULL,"<");
		tmp1 = strtok (NULL,">");
		int last = strtol(tmp1, &tmp2, 10);
		info->recordLastPage = last;

		tmp1 = strtok (NULL,"<");
		tmp1 = strtok (NULL,">");
		int tuples = strtol(tmp1, &tmp2, 10);
		info->numTuples = tuples;

		tmp1 = strtok (NULL,"<");
		tmp1 = strtok (NULL,">");
		int slot1 = strtol(tmp1, &tmp2, 10);
		info->slotSize = slot1;

		tmp1 = strtok (NULL,"<");
		tmp1 = strtok (NULL,">");
		int maxs = strtol(tmp1, &tmp2, 10);
		info->maxSlots = maxs;

		tmp1 = strtok (NULL,"<");
		tmp1 = strtok (NULL,">");
		int tnode_len = strtol(tmp1, &tmp2, 10);
		info->tNodeLen = tnode_len;
		tmp1 = strtok (NULL,"<");
		int j=0, page, slot;

		info->tstone_root = NULL;
		tNode *temp_node;

		while(j<tnode_len)
		{
			tmp1 = strtok (NULL,":");
			page = strtol(tmp1, &tmp2, 10);

			if(j == (tnode_len - 1))
			{
				tmp1 = strtok (NULL,">");
			}
			else
			{
				tmp1 = strtok (NULL,",");
			}
			slot = strtol(tmp1, &tmp2, 10);

			if (info->tstone_root != NULL)
			{
				temp_node->next = (tNode *)malloc(sizeof(tNode));
				temp_node->next->id.page = page;
				temp_node->next->id.slot = slot;
				temp_node = temp_node->next;
			}
			else
			{
				info->tstone_root = (tNode *)malloc(sizeof(tNode));
				info->tstone_root->id.page = page;
				info->tstone_root->id.slot = slot;
				temp_node = info->tstone_root;
			}
			++j;

		}
		return info;
	}
}

int getSchemaSize (Schema *schema)
{
	int flag=1;
	int total_size = 0,j = 0;
	while(flag)
	{
		total_size = sizeof(int)            // numAttr
		+ (schema->numAttr)*sizeof(int)     // dataTypes
		+ (schema->numAttr)*sizeof(int)     // type_lengths
		+ sizeof(int)                       // keySize
		+ (schema->keySize)*sizeof(int);    // keyAttrs

		while(j<schema->numAttr)
		{
			total_size += strlen(schema->attrNames[j]);
			++j;
		}
		flag=0;
	}
    return total_size;
}


Record *deserializeRecord(char *record_str, RM_TableData *rel)
{
	int flag=1;
	while(flag)
	{
		Schema *schema = rel->schema;
		tableInfo *infoptr = (tableInfo *) (rel->mgmtData);
		Value *value;
		Record *record = (Record *) malloc(sizeof(Record));


		record->data = (char *)malloc(sizeof(char) * infoptr->slotSize);
		char record_data[strlen(record_str)];
		strcpy(record_data, record_str);
		char *tmp1, *tmp2;

		tmp1 = strtok(record_data,"-");
		tmp1 = strtok (NULL,"]");
		tmp1 = strtok (NULL,"(");

		int j=0;

		while(j<schema->numAttr)
		{
			tmp1 = strtok (NULL,":");
			if(j != (schema->numAttr - 1))
			{
				tmp1 = strtok (NULL,",");
			}
			else
			{
				tmp1 = strtok (NULL,")");
			}

			/* set attribute values as per the attributes datatype */
			if(schema->dataTypes[j] == DT_INT)
			{
				int val = strtol(tmp1, &tmp2, 10);
				MAKE_VALUE(value, DT_INT, val);
				setAttr (record, schema, j, value);
				freeVal(value);
			}
			else if(schema->dataTypes[j] == DT_STRING)
			{
				MAKE_STRING_VALUE(value, tmp1);
				setAttr (record, schema, j, value);
				freeVal(value);
			}
			else if(schema->dataTypes[j] == DT_FLOAT)
			{
				float val = strtof(tmp1, NULL);
				MAKE_VALUE(value, DT_FLOAT, val);
				setAttr (record, schema, j, value);
				freeVal(value);
			}
			else if(schema->dataTypes[j] == DT_BOOL)
			{
				bool val;
				val = (tmp1[0] == 't') ? TRUE : FALSE;
				MAKE_VALUE(value, DT_BOOL, val);
				setAttr (record, schema, j, value);
				freeVal(value);
			}
			++j;
		}
		free(record_str);
		return record;
		flag=0;
	}
}

Schema *deserializeSchema(char *schema_str)
{
    Schema *schema = (Schema *) malloc(sizeof(Schema));
    int i=0, j=0;

    char schema_data[strlen(schema_str)];
    strcpy(schema_data, schema_str);

    char *tmp1, *tmp2;
    tmp1 = strtok (schema_data,"<");
    tmp1 = strtok (NULL,">");

    int attr = strtol(tmp1, &tmp2, 10);
    schema->numAttr= attr;

    schema->attrNames=(char **)malloc(sizeof(char*)*attr);
    schema->dataTypes=(DataType *)malloc(sizeof(DataType)*attr);
    schema->typeLength=(int *)malloc(sizeof(int)*attr);
    char* str_ref[attr];
    tmp1 = strtok (NULL,"(");

    while(j<attr)
	{
        tmp1 = strtok (NULL,": ");
        schema->attrNames[j]=(char *)calloc(strlen(tmp1), sizeof(char));
        strcpy(schema->attrNames[j], tmp1);

        if(j != attr-1)
		{
            tmp1 = strtok (NULL,", ");
        }
        else
		{
            tmp1 = strtok (NULL,") ");
        }

        str_ref[j] = (char *)calloc(strlen(tmp1), sizeof(char));

		int a = strcmp(tmp1, "INT");
		int f = strcmp(tmp1, "FLOAT");
		int b = strcmp(tmp1, "BOOL");
		int x=0;
        if (a == 0)
		{
            schema->dataTypes[j] = DT_INT;
            schema->typeLength[j] = 0;
			x=1;
        }
        if (f == 0)
		{
            schema->dataTypes[j] = DT_FLOAT;
            schema->typeLength[j] = 0;
			x=1;
        }
        if (b == 0)
		{
            schema->dataTypes[j] = DT_BOOL;
            schema->typeLength[j] = 0;
			x=1;
        }
        if(x == 0)
		{
            strcpy(str_ref[j], tmp1);
        }
		++j;
    }

    int keyFlag = 0, keySize = 0;
    char* keyAttrs[attr];

    if((tmp1 = strtok (NULL,"(")) != NULL)
	{
        tmp1 = strtok (NULL,")");
        char *key = strtok (tmp1,", ");

        while(key != NULL)
		{
            keyAttrs[keySize] = (char *)malloc(strlen(key)*sizeof(char));
            strcpy(keyAttrs[keySize], key);
            keySize++;
            key = strtok (NULL,", ");
        }
        keyFlag = 1;
    }

    char *temp3;
	j=0;
    while(j<attr)
	{
        if(strlen(str_ref[j]) > 0)
		{
            temp3 = (char *) malloc(sizeof(char)*strlen(str_ref[j]));
            memcpy(temp3, str_ref[j], strlen(str_ref[j]));
            schema->dataTypes[j] = DT_STRING;
            tmp1 = strtok (temp3,"[");
            tmp1 = strtok (NULL,"]");
            schema->typeLength[j] = strtol(tmp1, &tmp2, 10);
            free(temp3);
            free(str_ref[j]);
        }
		++j;
    }

    if(keyFlag == 1)
	{
        schema->keyAttrs=(int *)malloc(sizeof(int)*keySize);
        schema->keySize = keySize;
        while(j<keySize)
		{
			i=0;
            while(i<attr)
			{
                if(strcmp(keyAttrs[j], schema->attrNames[i]) == 0)
				{
                    schema->keyAttrs[j] = i;
                    free(keyAttrs[j]);
                }
				++i;
            }
			++j;
        }
    }

    return schema;
}


RC tableToFile(char *name, tableInfo *info)
{
	int flag =1;

	while(flag)
	{
		if(access(name, F_OK) == -1)
		{
			return RC_TABLE_NOT_FOUND;
		}

		SM_FileHandle fh;
		int stat;
		stat=openPageFile(name, &fh);
		if (stat != RC_OK)
		{
			return stat;
		}

		char *str = tableToStr(info);
		stat=writeBlock(0, &fh, str);
		if (stat != RC_OK)
		{
			free(str);
			return stat;
		}
		stat=closePageFile(&fh);
		if (stat != RC_OK)
		{
			free(str);
			return stat;
		}

		free(str);
		flag=0;
	}
		return RC_OK;
}

// table and manager
extern RC initRecordManager (void *mgmtData)
{
    return RC_OK;
}
extern RC shutdownRecordManager ()
{
    return RC_OK;
}

extern RC createTable (char *name, Schema *schema)
{
	int flag=1;
    /* Check if table already exists*/
	while(flag)
	{
		while( access(name, F_OK) != -1 )
		{
			return RC_TABLE_ALREADY_EXITS;
		}

		int status;
		status	= createPageFile(name);
		SM_FileHandle fh;
		// Create a file with the given name and create pages for info and schema
		while(status != RC_OK)
			return status;

		int sch_size = getSchemaSize(schema);
		int s_size = slotSize(schema);
		int f_size = (int) ceil((float)sch_size/PAGE_SIZE);
		int max_slots = (int) floor((float)PAGE_SIZE/(float)s_size);
		status = openPageFile(name, &fh);

		while (status != RC_OK)
			return status;
		status = ensureCapacity((f_size + 1), &fh);

		while (status != RC_OK)
		{
			return status;
		}
		// First page with file info
		tableInfo *infoptr = (tableInfo *) malloc(sizeof(tableInfo));
		infoptr->numTuples = 0;
		infoptr->slotSize = s_size;
		infoptr->maxSlots = max_slots;
		infoptr->recordStartPage = f_size + 1;
		infoptr->schemaLength = sch_size;
		infoptr->recordLastPage = f_size + 1;
		infoptr->tstone_root = NULL;

		char *info_str = tableToStr(infoptr);
		status = writeBlock(0, &fh, info_str);
		while (status != RC_OK)
			return status;

		// From next page, write the schema
		char *schema_str = serializeSchema(schema);
		status = writeBlock(1, &fh, schema_str);
		while (status != RC_OK)
			return status;
		status = closePageFile(&fh);
		while (status != RC_OK)
			return status;
		flag=0;
	}
    return RC_OK;
}

extern RC openTable (RM_TableData *rel, char *name)
{
	int flag=1;
	while(flag)
	{
    while(access(name, F_OK) == -1)
	{
        return RC_TABLE_NOT_FOUND;
    }

    BM_BufferPool *pool = (BM_BufferPool *)malloc(sizeof(BM_BufferPool));
    BM_PageHandle *bp = (BM_PageHandle *)malloc(sizeof(BM_PageHandle));

    initBufferPool(pool, name, 3, RS_FIFO, NULL);
    pinPage(pool, bp, 0);

    tableInfo *infoptr = strToTable(bp->data);

    if(infoptr->schemaLength < PAGE_SIZE)
	{
        pinPage(pool, bp, 1);
    }

    rel->schema = deserializeSchema(bp->data);
    rel->name = name;

    infoptr->bm = pool;
    rel->mgmtData = infoptr;

    free(bp);
	flag=0;
	}

    return RC_OK;
}

extern RC closeTable (RM_TableData *rel)
{
	int flag=1;
	while(flag)
	{
		shutdownBufferPool(((tableInfo *)rel->mgmtData)->bm);
		free(rel->schema->keyAttrs);
		free(rel->schema->dataTypes);
		free(rel->mgmtData);
		free(rel->schema->typeLength);
		free(rel->schema->attrNames);
		free(rel->schema);
		flag=0;
	}
	return RC_OK;
}

extern RC deleteTable (char *name)
{
	int flag=1;
	while(flag)
	{
		while(access(name, F_OK) == -1)
		{
			return RC_TABLE_NOT_FOUND;
		}

		while(remove(name) != 0)
		{
			return RC_TABLE_NOT_FOUND;
		}
		flag=0;
    }
    return RC_OK;
}

extern int getNumTuples (RM_TableData *rel)
{
	tableInfo *info = (tableInfo *)rel->mgmtData;
	int num = info->numTuples;
    return num;
}

// handling records in a table
extern RC insertRecord (RM_TableData *rel, Record *record)
{
	int flag=1;
    BM_PageHandle *pg = (BM_PageHandle *)malloc(sizeof(BM_PageHandle));
    tableInfo *info = (tableInfo *) (rel->mgmtData);
    int pgno, slot;

	while(flag)
	{
		if (info->tstone_root == NULL)
		{
			int start = info->recordStartPage;
			pgno = info->recordLastPage;
			int maxs = info->maxSlots;
			slot = info->numTuples - ((pgno - start)*maxs);

			while (slot == maxs)
			{
				slot = 0;pgno++;
			}
			info->recordLastPage = pgno;
		}
		else
		{
			tNode *node = (tNode *)info->tstone_root;
			pgno = node->id.page;
			slot = node->id.slot;
			info->tstone_root = node->next;
		}
		record->id.page = pgno;
		record->id.slot = slot;

		char *record_str = serializeRecord(record, rel->schema);

		pinPage(info->bm, pg, pgno);
		memcpy(pg->data + (slot*info->slotSize), record_str, strlen(record_str));
		free(record_str);

		if(flag)
		{
			markDirty(info->bm, pg);
			unpinPage(info->bm, pg);
			forcePage(info->bm, pg);
		}

		record->id.tstone = false;
		(info->numTuples)++;
		tableToFile(rel->name, info);
		free(pg);
		flag=0;
	}
	return RC_OK;
}

extern RC deleteRecord (RM_TableData *rel, RID id)
{
	int flag=1;
	while(flag)
	{
		tableInfo *info = (tableInfo *) (rel->mgmtData);
		tNode *tstone_iter = info->tstone_root;
		if(info->numTuples<0)
		{
			return RC_WRITE_FAILED;     //temp error. will update later.
		}
		else
		{
			/* add deleted RID to end of tombstone list */
			if(tstone_iter != NULL)
			{
				if (tstone_iter->next != NULL)
				{
					do{
						tstone_iter = tstone_iter->next;
					}while(tstone_iter->next != NULL);
				}
				tstone_iter->next = (tNode *)malloc(sizeof(tNode));
				tstone_iter = tstone_iter->next;
				tstone_iter->next = NULL;
			}
			else
			{
				info->tstone_root = (tNode *)malloc(sizeof(tNode));
				info->tstone_root->next = NULL;
				tstone_iter = info->tstone_root;
			}
			tstone_iter->id.slot = id.slot;
			tstone_iter->id.page = id.page;
			tstone_iter->id.tstone = TRUE;
			int tup = info->numTuples;
			tup--;
			(info->numTuples)=tup;
			tableToFile(rel->name, info);
		}
		flag=0;
	}

    return RC_OK;

}

extern RC updateRecord (RM_TableData *rel, Record *record)
{
	int flag=1;
	while(flag)
	{
		BM_PageHandle *pg = (BM_PageHandle *)malloc(sizeof(BM_PageHandle));
		int pgno = record->id.page;
		int slot = record->id.slot;
		tableInfo *info = (tableInfo *) (rel->mgmtData);
		BM_BufferPool *bp=(BM_BufferPool *)(info->bm);
		int size=info->slotSize;

		char *record_str = serializeRecord(record, rel->schema);

		pinPage(bp, pg, pgno);
		memcpy(pg->data + (slot*size), record_str, strlen(record_str));
		free(record_str);

		markDirty(bp, pg);
		unpinPage(bp, pg);
		forcePage(bp, pg);

		tableToFile(rel->name, info);
		flag=0;
	}
    return RC_OK;

}

extern RC getRecord (RM_TableData *rel, RID id, Record *record)
{
	int flag=1;
	while(flag)
	{
		tableInfo *info = (tableInfo *) (rel->mgmtData);
		BM_PageHandle *pg = (BM_PageHandle *)malloc(sizeof(BM_PageHandle));

		int pgno=id.page;
		int slot = id.slot;

		record->id.page = pgno;
		record->id.tstone = 0;
		record->id.slot = slot;

		/* Check if tombstone*/
		tNode *root = info->tstone_root;
		int tscount = 0;
		int tsflag = 0;

		int k=0;

		while(k<info->tNodeLen)
		{
			if (root->id.page == pgno && root->id.slot == slot)
			{
				tsflag = 1;
				record->id.tstone = 1;
				break;
			}
			root = root->next;
			tscount++;
			++k;
		}

		/* Read the page and slot*/

		if (tsflag == 0)
		{
			int start = info->recordStartPage;
			int maxs = info->maxSlots;
			int tuples = info->numTuples;
			int tupleNumber = (pgno - start)*(maxs) + slot + 1 - tscount;
			if (tupleNumber > tuples)
			{
				free(pg);
				return RC_RM_NO_MORE_TUPLES;
			}

			pinPage(info->bm, pg, pgno);
			int size = info->slotSize;
			char *record_str = (char *) malloc(sizeof(char) * size);
			memcpy(record_str, pg->data + ((slot)*size), sizeof(char)*size);
			unpinPage(info->bm, pg);

			Record *tmp = deserializeRecord(record_str, rel);

			record->data = tmp->data;
			free(tmp);
		}

		free(pg);
		flag=0;
	}
    return RC_OK;
}

// scans
extern RC startScan (RM_TableData *rel, RM_ScanHandle *scan, Expr *cond)
{
	int flag=1;
	while(flag)
	{
		/* initialize the RM_ScanHandle data structure */
		scan->rel = rel;
		tableInfo *info = (tableInfo *)rel->mgmtData;
		/* initialize node to store the information about the record to be searched and the condition to be evaluated */
		recordInfo *node = (recordInfo *) malloc(sizeof(recordInfo));
		node->curPage = info->recordStartPage;
		node->numSlots = info->maxSlots;
		node->curSlot = 0;
		node->numPages = info->recordLastPage;
		node->cond = cond;
		/* assign node to scan->mgmtData */
		scan->mgmtData = (void *) node;
		flag=0;
	}
    return RC_OK;
}

extern RC next (RM_ScanHandle *scan, Record *record)
{
	int flag=1;
	RC status;
	Value *val;
    recordInfo *node;
    node = scan->mgmtData;

	while(flag)
	{
		record->id.slot = node->curSlot;
		record->id.page = node->curPage;

		/* fetch the record as per the page and slot id */
		status = getRecord(scan->rel, record->id, record);

		if(status != RC_RM_NO_MORE_TUPLES)
		{
			/* check tombstone id for a deleted record and update record node parameters accordingly */
			if(record->id.tstone != 1)
			{
				evalExpr(record, scan->rel->schema, node->cond, &val);
				if (node->curSlot != node->numSlots - 1)
				{
					(node->curSlot)++;
				}
				else
				{
					node->curSlot = 0;
					(node->curPage)++;
				}
				scan->mgmtData = node;

				/* If the record fetched is not the required one then call the next function with the updated record id parameters. */
				if(val->v.boolV == 1)
				{
					return RC_OK;
				}
				else
				{
					return next(scan, record);
				}
			}
			/* evaluate the given expression to check if the record is the one required by the client */
			else
			{
				if (node->curSlot != node->numSlots - 1)
				{
					(node->curSlot)++;
				}
				else
				{
					node->curSlot = 0;
					(node->curPage)++;
				}
				scan->mgmtData = node;
				return next(scan, record);
			}
		}
		else
		{
			return RC_RM_NO_MORE_TUPLES;
		}
		flag=0;
	}

}

extern RC closeScan (RM_ScanHandle *scan)
{
    return RC_OK;
}

// dealing with schemas
extern int getRecordSize (Schema *schema)
{
	int flag=1;
	int s = 0, ts = 0, i=0;
	if(flag)
	{
		while(i<schema->numAttr)
		{
			if (schema->dataTypes[i] == DT_STRING)
			{
				ts = schema->typeLength[i];
			}
			else if(schema->dataTypes[i] == DT_INT)
			{
				ts = sizeof(int);
			}
			else if(schema->dataTypes[i] == DT_FLOAT)
			{
				ts = sizeof(float);
			}
			else if(schema->dataTypes[i] == DT_BOOL)
			{
				ts = sizeof(bool);
			}
			s += ts;
			++i;
		}
	}
	return s;
}

extern Schema *createSchema (int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys)
{
	int flag=1;
    Schema *sch = (Schema *) malloc(sizeof(Schema));
    sch->numAttr = numAttr;
    sch->keyAttrs = keys;
	sch->typeLength = typeLength;
	if(flag)
	{
		sch->dataTypes = dataTypes;
		sch->attrNames = attrNames;
		sch->keySize = keySize;
	}
    return sch;
}

extern RC freeSchema (Schema *schema)
{
    free(schema);
    return RC_OK;
}

// dealing with records and attribute values
extern RC createRecord (Record **record, Schema *schema)
{
	int flag=1;
	while(flag)
	{
		int attr = schema->numAttr;
		DataType *dt = schema->dataTypes;
		int *length = schema->typeLength;
		int x=0;
		int mem = 0;
		char *data;
		while (x < attr)
		{
			int d = *(dt + x);
			switch(d)
			{
				case DT_INT:
				{
					mem += sizeof(int);
				}
				break;
				case DT_FLOAT:
				{
					mem += sizeof(float);
				}
				break;
				case DT_BOOL:
				{
					mem += sizeof(bool);
				}
				break;
				case DT_STRING:
				{
					mem += *(length + x);
				}
				break;
			}
			x++;
		}
		data= (char *)malloc(mem + attr);
		x=0;
		while(x < mem + attr)
		{
			data[x]='\0';
			x++;
		}
		*record = (Record *)malloc(sizeof(Record));
		record[0]->data=data;//create a char with length equals length of the tuple.
		flag=0;
	}
	return RC_OK;
}

extern RC freeRecord (Record *record)
{
	/* free the memory space allocated to record and its data */
	int flag=1;
	while(flag)
	{
		free(record->data);
		free(record);
		flag=0;
	}
    return RC_OK;
}

extern RC getAttr (Record *record, Schema *schema, int attrNum, Value **value)
{
	int flag=1;
	while(flag)
	{
		/* allocate the space to the value data structre where the attribute values are to be fetched */
		*value = (Value *)  malloc(sizeof(Value));
		int offset;
		char *data;

		/* get the offset value of different attributes as per the attribute numbers */
		attrOffset(schema, attrNum, &offset);
		data = (record->data + offset);
		(*value)->dt = schema->dataTypes[attrNum];

		/* attribute data is assigned to the value pointer as per the different data types */
		if(schema->dataTypes[attrNum] == DT_INT)
		{
			memcpy(&((*value)->v.intV),data, sizeof(int));
		}
		else if(schema->dataTypes[attrNum] == DT_STRING)
		{
			int length = schema->typeLength[attrNum];
			char *str;
			str = (char *) malloc(length + 1);
			strncpy(str, data, length);
			str[length] = '\0';
			(*value)->v.stringV = str;
		}
		else if(schema->dataTypes[attrNum] == DT_FLOAT)
		{
			memcpy(&((*value)->v.floatV),data, sizeof(float));
		}
		else if(schema->dataTypes[attrNum] == DT_BOOL)
		{
			memcpy(&((*value)->v.boolV),data, sizeof(bool));
		}
		flag=0;
	}
    return RC_OK;

}

extern RC setAttr (Record *record, Schema *schema, int attrNum, Value *value)
{
	int flag=1;
	while(flag)
	{
		int offset;
		char *data;

		/* get the offset value of different attributes as  per the attribute numbers */
		attrOffset(schema, attrNum, &offset);
		data = record->data + offset;

		/* set attribute values as per the attributes datatype */
		if(schema->dataTypes[attrNum] == DT_INT)
		{
			memcpy(data,&(value->v.intV), sizeof(int));
		}
		else if(schema->dataTypes[attrNum] == DT_STRING)
		{
			char *str;
			int length = schema->typeLength[attrNum];
			str = (char *) malloc(length);
			str = value->v.stringV;
			memcpy(data,(str), length);
		}
		else if(schema->dataTypes[attrNum] == DT_FLOAT)
		{
			memcpy(data,&((value->v.floatV)), sizeof(float));
		}
		else if(schema->dataTypes[attrNum] == DT_BOOL)
		{
			memcpy(data,&((value->v.boolV)), sizeof(bool));
		}
		flag=0;
	}
    return RC_OK;
}
