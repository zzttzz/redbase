//
// rm.h
//
//   Record Manager component interface
//
// This file does not include the interface for the RID class.  This is
// found in rm_rid.h
//

#ifndef RM_H
#define RM_H

// Please DO NOT include any files other than redbase.h and pf.h in this
// file.  When you submit your code, the test program will be compiled
// with your rm.h and your redbase.h, along with the standard pf.h that
// was given to you.  Your rm.h, your redbase.h, and the standard pf.h
// should therefore be self-contained (i.e., should not depend upon
// declarations in any other file).

// Do not change the following includes
#include "redbase.h"
#include "rm_rid.h"
#include "pf.h"

#include <cassert>

//
// RM_FileHdr: Header structure for files
//
struct RM_FileHdr {
   int firstFree;     // first free page in the linked list
   int numPages;      // # of pages in the file
	 int extRecordSize;  // record size as seen by users.
};

class bitmap {
 public:
	bitmap(int numBits);
	bitmap(char * buf, int numBits); //deserialize from buf
	~bitmap();

	bool set(unsigned int bitNumber);
	bool set(); // set all bits to 1
	bool reset(unsigned int bitNumber);
	bool reset(); // set all bits to 0
	bool test(unsigned int bitNumber) const;

	int numChars() const; // return size of char buffer to hold bitmap
	int to_char_buf(char *, int len) const; //serialize content to char buffer
	int getSize() const { return size; } 
 private:
	int size;
	char * buffer;
};

ostream& operator <<(ostream & os, const bitmap& b);

//
// RM_PageHdr: Header structure for pages
//
#define RM_PAGE_LIST_END  -1       // end of list of free pages
#define RM_PAGE_FULLY_USED      -2       // page is fully used with no free slots
                                   // not a member of the free list
struct RM_PageHdr {
	int nextFree;       // nextFree can be any of these values:
	                    //  - the number of the next free page
	                    //  - RM_PAGE_LIST_END if this is last free page
                      //  - RM_PAGE_FULLY_USED if the page is not free
	char * freeSlotMap; // A bitmap that tracks the free slots within 
	                    // the page
	int numSlots;
	int numFreeSlots;

  RM_PageHdr(int numSlots) : numSlots(numSlots), numFreeSlots(numSlots)
	{ assert(numSlots > 0); freeSlotMap = new char[this->mapsize()];}

  ~RM_PageHdr()
	{ delete freeSlotMap; }

	int size() const 
	{ return sizeof(nextFree) + sizeof(numSlots) + sizeof(numFreeSlots)
			+ bitmap(numSlots).numChars()*sizeof(char); }
	int mapsize() const 
	{ return this->size() - sizeof(nextFree)
			- sizeof(numSlots) - sizeof(numFreeSlots);}
	int to_buf(char *& buf) const
	{ 
		memcpy(buf, &nextFree, sizeof(nextFree));
		memcpy(buf + sizeof(nextFree), &numSlots, sizeof(numSlots));
		memcpy(buf + sizeof(nextFree) + sizeof(numSlots), 
					 &numFreeSlots, sizeof(numFreeSlots));
		memcpy(buf + sizeof(nextFree) + sizeof(numSlots) + sizeof(numFreeSlots),
					 freeSlotMap, this->mapsize()*sizeof(char));
	}
	int from_buf(const char * buf)
	{
		memcpy(&nextFree, buf, sizeof(nextFree));
		memcpy(&numSlots, buf + sizeof(nextFree), sizeof(numSlots));
		memcpy(&numFreeSlots, buf + sizeof(nextFree) + sizeof(numSlots),
					 sizeof(numFreeSlots));
		memcpy(freeSlotMap, 
					 buf + sizeof(nextFree) + sizeof(numSlots) + sizeof(numFreeSlots),
					 this->mapsize()*sizeof(char));
	}
};

//
// RM_Record: RM Record interface
//
class RM_Record {
	friend class RM_RecordTest;
  public:
    RM_Record ();
    ~RM_Record();

    // Return the data corresponding to the record.  Sets *pData to the
    // record contents.
    RC GetData(char *&pData) const;

		// Sets data in the record for a fixed recordsize of size.
    // Real object is only available and usable at this point not after
    // construction
    RC Set(char *pData, int size, RID id);

    // Return the RID associated with the record
    RC GetRid (RID &rid) const;
  private:
		int recordSize;
		char * data;			
};

//
// RM_FileHandle: RM File interface
//
class RM_FileHandle {
	friend class RM_FileHandleTest;
	friend class RM_FileScan;
	friend class RM_Manager;
public:
    RM_FileHandle ();
    RC Open(PF_FileHandle*, int recordSize);
		RC SetHdr(RM_FileHdr h) { hdr = h; return 0;}
    ~RM_FileHandle();

    // Given a RID, return the record
    RC GetRec     (const RID &rid, RM_Record &rec) const;

    RC InsertRec  (const char *pData, RID &rid);       // Insert a new record

    RC DeleteRec  (const RID &rid);                    // Delete a record
    RC UpdateRec  (const RM_Record &rec);              // Update a record

    // Forces a page (along with any contents stored in this class)
    // from the buffer pool to disk.  Default value forces all pages.
    RC ForcePages (PageNum pageNum = ALL_PAGES);

		RC GetPF_FileHandle(PF_FileHandle &) const;
		bool hdrChanged() const { return bHdrChanged; }
		int GetFullRecordSize() const { return fullRecordSize; }
		int GetNumPages() const { return hdr.numPages; }
		int GetNumSlots() const;

 private:
		bool IsValidPageNum (const PageNum pageNum) const;
		bool IsValidRID(const RID rid) const;

		// Return next free page or allocate one as needed.
	 RC GetNextFreePage(PageNum& pageNum);
	 RC GetNextFreeSlot(PF_PageHandle& ph, PageNum& pageNum, SlotNum&);
	 RC GetPageHeader(PF_PageHandle ph, RM_PageHdr & pHdr) const;
	 RC SetPageHeader(PF_PageHandle ph, const RM_PageHdr& pHdr);
	 RC GetSlotPointer(PF_PageHandle ph, SlotNum s, char *& pData) const;

	 // write hdr member using a newly open file's header page
	 RC GetFileHeader(PF_PageHandle ph);
	 // persist header into the first page of a file for later
	 RC SetFileHeader(PF_PageHandle ph) const;

   PF_FileHandle *pfHandle;                       // pointer to opened PF_FileHandle
   RM_FileHdr hdr;                                // file header
	 int fullRecordSize; // external + RID

	 bool bFileOpen;                                // file open flag
   bool bHdrChanged;                              // dirty flag for file hdr
};

class Predicate {
 public:
	Predicate() {}
	~Predicate() {}

	Predicate(                     AttrType   attrTypeIn,	     
																 int        attrLengthIn,
																 int        attrOffsetIn,
																 CompOp     compOpIn,
																 void       *valueIn,
																 ClientHint pinHintIn) 
		{
			attrType = attrTypeIn;	     
			attrLength = attrLengthIn;
			attrOffset = attrOffsetIn;
			compOp = compOpIn;
			value = valueIn;
			pinHint = pinHintIn;
		}

	bool eval(const char *buf, CompOp c) const {
		if(c == NO_OP) {
			return true;
		}
		const char * attr = buf + attrOffset;
		
		if(c == LT_OP) {
			if(attrType == INT) {
				return *attr < *((int *)value);
			}
			if(attrType == FLOAT) {
				return *attr < *((float *)value);
			}
			if(attrType == STRING) {
				return strncmp(attr, (char *)value, attrLength) < 0;
			}
		}
		if(c == GT_OP) {
			if(attrType == INT) {
				return *attr > *((int *)value);
			}
			if(attrType == FLOAT) {
				return *attr > *((float *)value);
			}
			if(attrType == STRING) {
				return strncmp(attr, (char *)value, attrLength) > 0;
			}
		}
		if(c == EQ_OP) {
			if(attrType == INT) {
				return *attr == *((int *)value);
			}
			if(attrType == FLOAT) {
				return *attr == *((float *)value);
			}
			if(attrType == STRING) {
				return strncmp(attr, (char *)value, attrLength) == 0;
			}
		}
		if(c == LE_OP) {
			return this->eval(buf, LT_OP) || this->eval(buf, EQ_OP); 
		}
		if(c == GE_OP) {
			return this->eval(buf, GT_OP) || this->eval(buf, EQ_OP); 
		}
		if(c == NE_OP) {
			return !this->eval(buf, EQ_OP);
		}

	}

	CompOp initOp() const { return compOp; }



 private:
	AttrType   attrType;
	int        attrLength;
	int        attrOffset;
	CompOp     compOp;
	void*      value;
	ClientHint pinHint;
};

//
// RM_FileScan: condition-based scan of records in the file
//
class RM_FileScan {
public:
    RM_FileScan  ();
    ~RM_FileScan ();

    RC OpenScan  (const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint = NO_HINT); // Initialize a file scan
    RC GetNextRec(RM_Record &rec);               // Get next matching record
    RC CloseScan ();                            // Close the scan
 private:
		Predicate * pred;
		RM_FileHandle * prmh;
		RID current;
		bool bOpen;
};

//
// RM_Manager: provides RM file management
//
class RM_Manager {
public:
    RM_Manager    (PF_Manager &pfm);
    ~RM_Manager   ();

    RC CreateFile (const char *fileName, int recordSize);
    RC DestroyFile(const char *fileName);
    RC OpenFile   (const char *fileName, RM_FileHandle &fileHandle);

    RC CloseFile  (RM_FileHandle &fileHandle);

private:
    PF_Manager&   pfm; // A reference to the external PF_Manager
};

//
// Print-error function
//
void RM_PrintError(RC rc);

#define RM_LASTWARN START_RM_WARN

#define RM_SIZETOOBIG      (START_RM_ERR - 0)  // record size too big
#define RM_PF              (START_RM_ERR - 1)  // error in PF
#define RM_NULLRECORD      (START_RM_ERR - 2)  
#define RM_RECSIZEMISMATCH (START_RM_ERR - 3)  // record size mismatch
#define RM_HANDLEOPEN      (START_RM_ERR - 4)
#define RM_BADOPEN         (START_RM_ERR - 5)
#define RM_FNOTOPEN        (START_RM_ERR - 6)
#define RM_BAD_RID         (START_RM_ERR - 7)
#define RM_EOF             (START_RM_ERR - 8)  // end of file

#define RM_LASTERROR RM_EOF

#endif // RM_H
