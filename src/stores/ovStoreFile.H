
/******************************************************************************
 *
 *  This file is part of canu, a software program that assembles whole-genome
 *  sequencing reads into contigs.
 *
 *  This software is based on:
 *    'Celera Assembler' (http://wgs-assembler.sourceforge.net)
 *    the 'kmer package' (http://kmer.sourceforge.net)
 *  both originally distributed by Applera Corporation under the GNU General
 *  Public License, version 2.
 *
 *  Canu branched from Celera Assembler at its revision 4587.
 *  Canu branched from the kmer project at its revision 1994.
 *
 *  This file is derived from:
 *
 *    src/stores/ovStore.H
 *
 *  Modifications by:
 *
 *    Brian P. Walenz from 2014-DEC-09 to 2015-JUL-01
 *      are Copyright 2014-2015 Battelle National Biodefense Institute, and
 *      are subject to the BSD 3-Clause License
 *
 *    Brian P. Walenz beginning on 2015-OCT-12
 *      are a 'United States Government Work', and
 *      are released in the public domain
 *
 *    Sergey Koren beginning on 2016-MAR-11
 *      are a 'United States Government Work', and
 *      are released in the public domain
 *
 *  File 'README.licenses' in the root directory of this distribution contains
 *  full conditions and disclaimers for each license.
 */

#ifndef AS_OVSTOREFILE_H
#define AS_OVSTOREFILE_H

#include "AS_global.H"
#include "sqStore.H"

#include "ovOverlap.H"

class ovStoreHistogram;


#define  OVFILE_MAX_OVERLAPS  (1024 * 1024 * 1024 / (sizeof(ovOverlapDAT) + sizeof(uint32)))


//  The default, no flags, is to open for normal overlaps, read only.  Normal overlaps mean they
//  have only the B id, i.e., they are in a fully built store.
//
//  Output of overlapper (input to store building) should be ovFileFullWrite.  The specialized
//  ovFileFullWriteNoCounts is used internally by store creation.
//
enum ovFileType {
  ovFileNormal              = 0,  //  Reading of b_id overlaps (aka store files)
  ovFileNormalWrite         = 1,  //  Writing of b_id overlaps
  ovFileFull                = 2,  //  Reading of a_id+b_id overlaps (aka overlapper output files)
  ovFileFullCounts          = 3,  //  Reading of a_id+b_id overlaps (but only loading the count data, no overlaps)
  ovFileFullWrite           = 4,  //  Writing of a_id+b_id overlaps
  ovFileFullWriteNoCounts   = 5   //  Writing of a_id+b_id overlaps, omitting the counts of olaps per read
};


//  For overlaps out of an overlapper, stored in ovFileFull, we want to keep the number
//  of overlaps per read.  For simplicity, we keep the number of overlaps
//  for all reads, not just those with overlaps.

class ovFileOCW {
public:
  ovFileOCW(sqStore *seq, const char *prefix) {

    memset(_name, 0, sizeof(char) * (FILENAME_MAX+1));

    _nOlaps = 0;
    _oprMax = 0;
    _opr    = NULL;

    //  If no prefix supplied, we're being constructed to count
    //  only the number of overlaps.  Leave the array empty.
    //
    //  Otherwise, we want to keep track of the number of overlaps
    //  per read, too.

    if (prefix != NULL) {
      snprintf(_name, FILENAME_MAX, "%s.oc", prefix);

      _oprMax = seq->sqStore_getNumReads() + 1;

      allocateArray(_opr, _oprMax, resizeArray_clearNew);
    }
  };

  ~ovFileOCW() {

    if (_opr == NULL)
      return;

    //  Even if there are no overlaps (_nOlaps == 0), save the counts file.  If this isn't
    //  saved, we'd need to special case a whole bunch of stuff that assumes counts exist
    //  (e.g., in ovStoreConfig).

    FILE   *F = AS_UTL_openOutputFile(_name);

    AS_UTL_safeWrite(F, &_nOlaps, "ovStoreHistogram::nr",  sizeof(uint32), 1);
    AS_UTL_safeWrite(F, &_oprMax, "ovStoreHistogram::nr",  sizeof(uint32), 1);
    AS_UTL_safeWrite(F,  _opr,    "ovStoreHistogram::opr", sizeof(uint32), _oprMax);

    fclose(F);

    delete [] _opr;
  };

  void          addOverlap(ovOverlap *overlap) {

    _nOlaps++;

    if (_opr == NULL)   //  No _opr for writing store overlap files, but
      return;           //  we still need to get _nOlaps for fileTooBig().

    assert(overlap->a_iid < _oprMax);
    assert(overlap->b_iid < _oprMax);

    _opr[overlap->a_iid]++;
    _opr[overlap->b_iid]++;
  };

  uint32        numOverlaps(void)           { return(_nOlaps);      };

private:
  uint64       _nOlaps;   //  Total number of overlaps stored in this file.
  uint32       _oprMax;   //  Length of _opr; number of reads + 1.
  uint32      *_opr;      //  Overlaps per read - assumes only one overlap per (a,b) read pair.

  char         _name[FILENAME_MAX+1];
};




class ovFileOCR {
public:
  ovFileOCR(sqStore *seq, const char *prefix) {

    //  If no prefix supplied, there just aren't any
    //  overlap counts expected with this input, so
    //  leave things empty.

    _nOlaps = 0;
    _oprMax = 0;
    _opr    = NULL;

    if (prefix == NULL)
      return;

    //  Otherwise, look for the overlap counts file.

    char  name[FILENAME_MAX+1];
    snprintf(name, FILENAME_MAX, "%s.oc", prefix);

    //  If no counts file, then this must be an intermediate overlap file (and the user
    //  should have sent in a NULL prefix!) used when constructing the store.
    //  Return with no counts allocated and trust that the user doesn't try to access them.

    if (AS_UTL_fileExists(name) == false)
      return;

    //  Otherwise, counts exist, and we load them.

    _oprMax = seq->sqStore_getNumReads() + 1;

    allocateArray(_opr, _oprMax, resizeArray_clearNew);

    FILE   *F = AS_UTL_openInputFile(name);

    AS_UTL_safeRead(F, &_nOlaps, "ovStoreHistogram::nr",  sizeof(uint32), 1);
    AS_UTL_safeRead(F, &_oprMax, "ovStoreHistogram::nr",  sizeof(uint32), 1);
    AS_UTL_safeRead(F,  _opr,    "ovStoreHistogram::opr", sizeof(uint32), _oprMax);

    fclose(F);
  };

  ~ovFileOCR() {
    delete [] _opr;
  };

  uint32        numOverlaps(void)           { return(_nOlaps);      };
  uint32        numOverlaps(uint32 readID)  { return(_opr[readID]); };

private:
  uint64       _nOlaps;   //  Total number of overlaps here.
  uint32       _oprMax;   //  Length of _opr; number of reads + 1.
  uint32      *_opr;      //  Overlaps per read.
};





class ovFile {
public:
  ovFile(sqStore     *seq,
         const char  *fileName,
         ovFileType   type = ovFileNormal,
         uint32       bufferSize = 1 * 1024 * 1024);

  ovFile(sqStore     *seq,
         const char  *ovlName,
         uint32       sliceNum,
         uint32       pieceNum,
         ovFileType   type = ovFileNormal,
         uint32       bufferSize = 1 * 1024 * 1024);

  ~ovFile();

private:
  void    construct(sqStore *seqName, const char *fileName, ovFileType type, uint32 bufferSize);

public:
  static
  char   *createDataName(char *name, const char *storeName, uint32 slice, uint32 piece);

public:
  void    writeBuffer(bool force=false);
  void    writeOverlap(ovOverlap *overlap);
  void    writeOverlaps(ovOverlap *overlaps, uint64 overlapLen);

  bool    fileTooBig(void)    { return(_countsW->numOverlaps() > OVFILE_MAX_OVERLAPS);  };
  uint64  filePosition(void)  { return(_countsW->numOverlaps());                        };

  void    readBuffer(void);
  bool    readOverlap(ovOverlap *overlap);
  uint64  readOverlaps(ovOverlap *overlaps, uint64 overlapMax);

  void    seekOverlap(off_t overlap);

  //  The size of an overlap record is 1 or 2 IDs + the size of a word times the number of words.
  uint64  recordSize(void) {
    return(sizeof(uint32) * ((_isNormal) ? 1 : 2) + sizeof(ovOverlapWORD) * ovOverlapNWORDS);
  };

  //  Used primarily for copying the data from this file into the data for the full overlap store.
  ovStoreHistogram       *getHistogram(void)     { return(_histogram); };
  void                    removeHistogram(void);

  ovFileOCR              *getCounts(void)        { return(_countsR);   };

private:
  sqStore                *_seq;

  ovFileOCW              *_countsW;
  ovFileOCR              *_countsR;
  ovStoreHistogram       *_histogram;

  uint32                  _bufferLen;    //  length of valid data in the buffer
  uint32                  _bufferPos;    //  position the read is at in the buffer
  uint32                  _bufferMax;    //  allocated size of the buffer
  uint32                 *_buffer;

  size_t                  _snappyLen;
  char                   *_snappyBuffer;

  bool                    _isOutput;     //  if true, we can writeOverlap()
  bool                    _isNormal;     //  if true, 3 words per overlap, else 4
  bool                    _useSnappy;    //  if true, compress with snappy before writing

  char                    _prefix[FILENAME_MAX+1];
  char                    _name[FILENAME_MAX+1];
  FILE                   *_file;
};


#endif  //  AS_OVSTOREFILE_H
