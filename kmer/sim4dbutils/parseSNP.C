#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>

#include "bio.h"
#include "sim4.H"

//  Writes things with mappings that don't contain the snp itself to a
//  failure file.  Otherwise, if the mapping is above the threshold, a
//  line describing the snp is output.

sim4polishWriter  *multiMultiFile   = 0L;  //  multiple hits, at least one is multiple exon
sim4polishWriter  *multiSingleFile  = 0L;  //  multiple hits, all are single exon
sim4polishWriter  *singleMultiFile  = 0L;  //  single hit, and it has more than one exon
sim4polishWriter  *singleSingleFile = 0L;  //  single hit, single exon

int               smpass = 0;
int               sspass = 0;
int               mmpass = 0;
int               mspass = 0;

int               smfail = 0;
int               ssfail = 0;
int               mmfail = 0;
int               msfail = 0;

int               failedsnps    = 0;
int               failedmatches = 0;

FILE             *validSNPMapFile   = 0L;
sim4polishWriter *failedSNPMapFile  = 0L;

char              fieldDelimiter = 0;
const char       *sizeTag        = "/size=";
const char       *posTag         = "/pos=";
int               positionOffset = 0;

int               outputFormat   = 1;

static
char *
findSNPid(char *defline) {
  char *ret = 0L;
  int   sta = 0;
  int   len = 0;
  int   i = 0;

  if (fieldDelimiter == 0) {
    for (len=1; defline[len] && !isspace(defline[len]); len++)
      ;
  } else {
    for (len=1; defline[len] && defline[len] != fieldDelimiter; len++)
      ;
  }

#if 0
  //  This was used for a set of SNPs with a non-standard defline
  //  structure.  It returns the field between the first '|' and the
  //  next '_'.
  //
  for (len=1; defline[len] && defline[len] != '_'; len++)
    ;
  for (sta=len-1; sta > 0 && defline[sta] != '|'; sta--)
    ;
#endif

  ret = new char [len+1];

  for (i=sta; i<len-1; i++)
    ret[i-sta] = defline[i+1];

  ret[len-sta-1] = 0;

  return(ret);
}

static
char *
findGENid(char *defline) {
  char *ret = 0L;
  int   len = 0;
  int   i = 0;

  for (len=1; defline[len] && !isspace(defline[len]); len++)
    ;

  ret = new char [len+1];

  for (i=0; i<len-1; i++)
    ret[i] = defline[i+1];

  ret[len-1] = 0;

  return(ret);
}



static
int
findPosition(char *defline) {
  char *p = 0L;

  p = strstr(defline, posTag);

  //  Look for standard posTags if we didn't find the one the user wanted.

  if (p == 0L)
    p = strstr(defline, "allelePos=");
  if (p == 0L)
    p = strstr(defline, "/pos=");
  if (p == 0L)
    fprintf(stderr, "posTag '%s' (also looked for 'allelePos=' and '/pos=') not found in defline '%s'!\n", posTag, defline), exit(1);

  while (*p && !isdigit(*p))
    p++;

  if (*p == 0) {
    fprintf(stderr, "Found posTag '%s' in defline '%s', but didn't find any numbers!\n", posTag, defline);
    exit(1);
  }

  return(atoi(p) + positionOffset);
}




//  Returns 1 if SNP was valid and printed,
//  0 otherwise.
//
static
int
printSNP(FILE *F, sim4polish *p) {
  uint32   pos           = findPosition(p->_estDefLine);
  uint32   exonWithSNP   = ~uint32ZERO;
  uint32   i             = 0;
  uint32   seqOffset     = 0;

  //  If the match is complement, then the alignment is printed using
  //  the reverse complemented SNP sequence, and so we need to find
  //  the offset at the end of the sequence (not always the same as
  //  the offset at the start of the sequence).
  //
  //  XXX: Previous version had this as "p->_estLen - pos + siz", which
  //  seems wrong.  This version does what appears to be reverse
  //  complement - size.  I don't understand if this is a "size" or
  //  just a "1" thing.
  //
  seqOffset = pos;
  if (p->_matchOrientation == SIM4_MATCH_COMPLEMENT)
    seqOffset = p->_estLen - pos - 1;

  //  Find the exon with the SNP
  //
  for (i=0; i<p->_numExons; i++)
    if (((p->_exons[i]._estFrom-1) <= seqOffset) && (seqOffset <= (p->_exons[i]._estTo-1)))
      exonWithSNP = i;

  if (exonWithSNP == ~uint32ZERO)
    return(0);

  //  If we are printing to a file, continue to find the location, otherwise,
  //  just return.
  //
  if (F) {
    char *SNPid = findSNPid(p->_estDefLine);
    char *GENid = findGENid(p->_genDefLine);

    char  SNPbase = 0;
    char  GENbase = 0;

    //  Now, we examine the alignment strings to decide exactly
    //  where the SNP is located in the genomic.
    //
    //  bpToExaine - the number of bases we need to skip in the
    //  alignment (counted in the snp), +1 because we are currently at
    //  the bp before the alignment (so we need to skip one more space).
    //
    //  XXX:  these used to be int!
    //
    uint32  bpToExamine = seqOffset - (p->_exons[exonWithSNP]._estFrom - 1) + 1;
    uint32  examinePos  = 0;
    uint32  genPosition = p->_exons[exonWithSNP]._genFrom - 1;

    //  Recent runs of dbSNP showed that we are off by one (too many if forward, too few if complement).  This is a hack to fix it.
    //
    if (p->_matchOrientation == SIM4_MATCH_COMPLEMENT)
      bpToExamine++;
    else
      bpToExamine--;

    while (bpToExamine > 0) {

      //  If the SNP alignment eats up a base pair, decrement
      //  the number of bp left to examine.
      //
      if (p->_exons[exonWithSNP]._estAlignment[examinePos] != '-')
        bpToExamine--;

      //  If the the genomic alignment is not a gap, increment the
      //  position.
      //
      if (p->_exons[exonWithSNP]._genAlignment[examinePos] != '-')
        genPosition++;

      examinePos++;
    }

    //  Adjust the quality values, treating the SNP as a match always.
    //
    SNPbase = p->_exons[exonWithSNP]._estAlignment[examinePos-1];
    GENbase = p->_exons[exonWithSNP]._genAlignment[examinePos-1];

    p->_exons[exonWithSNP]._estAlignment[examinePos-1] = 'A';
    p->_exons[exonWithSNP]._genAlignment[examinePos-1] = 'A';

    p->s4p_updateAlignmentScores();

    p->_exons[exonWithSNP]._estAlignment[examinePos-1] = SNPbase;
    p->_exons[exonWithSNP]._genAlignment[examinePos-1] = GENbase;


    if (outputFormat == 1) {
      fprintf(F, "%s %s "uint32FMT" %c/%c %s global["uint32FMT" "uint32FMT"] exon["uint32FMT" "uint32FMT" "uint32FMT" "uint32FMT"]\n",
              SNPid,
              GENid,
              genPosition,
              SNPbase,
              GENbase,
              (p->_matchOrientation == SIM4_MATCH_FORWARD) ? "forward" : "complement",
              p->_percentIdentity,
              p->_querySeqIdentity,
              p->_numExons,
              exonWithSNP,
              p->_exons[exonWithSNP]._percentIdentity,
              (uint32)floor(100.0 * (double)p->_exons[exonWithSNP]._numMatches / (double)p->_estLen));
    } else if (outputFormat == 2) {

      //  The format is all on one line, data fields separated by tab.
      //  No spaces -- "sa=C" instead of "sa = C"
      //
      //    SNPid
      //    GENid
      //    genomic position of SNP
      //    sa=c        -- snp allele
      //    ga=c        -- genome allele
      //    mo={f|r}    -- mapping orientation
      //    pi=n        -- percent identity
      //    pc=n        -- percent coverage
      //    nb=n        -- number of alignment blocks
      //    bl=n        -- alignment block with the snp
      //    bp=n        -- position of the snp in the alignment block
      //    bi=n        -- percent identity of the block
      //    bc=n        -- percent coverage of the block
      //
      //  The first three items are mandatory, are always in that
      //  order, and are always the first three.  The others are
      //  optional, and can occur in any order.  There might be more
      //  present than listed here.
      //
      //  The order and content should be consistent for any given
      //  version of the software.
      //
      fprintf(F, "%s %s "uint32FMT" sa=%c ga=%c mo=%c pi="uint32FMT" pc="uint32FMT" nb="uint32FMT" bl="uint32FMT" bp="uint32FMT" bi="uint32FMT" bc="uint32FMT"\n",
              "a", //SNPid,
              "b", //GENid,
              genPosition,
              p->_exons[exonWithSNP]._estAlignment[examinePos-1],                                        // sa
              p->_exons[exonWithSNP]._genAlignment[examinePos-1],                                        // ga
              (p->_matchOrientation == SIM4_MATCH_FORWARD) ? 'f' : 'r',                                  // mo
              p->_percentIdentity,                                                                       // pi
              p->_querySeqIdentity,                                                                      // pc
              p->_numExons,                                                                              // nb
              exonWithSNP,                                                                               // bl
              examinePos,                                                                                // bp
              p->_exons[exonWithSNP]._percentIdentity,                                                   // bi
              (uint32)floor(100.0 * (double)p->_exons[exonWithSNP]._numMatches / (double)p->_estLen));   // bc
    } else {
    }

    delete [] SNPid;
    delete [] GENid;
  }

  return(1);
}



//  Just a wrapper around the real best picker, so that we can easily
//  destroy polishes when we're done.
//
static
void
parseSNP(sim4polish **p, int pNum) {
  int   numMulti  = 0;
  int   i;

  //  Count the number of matches that have more than one exon
  //
  for (i=0; i<pNum; i++)
    if (p[i]->_numExons > 1)
      numMulti++;

  if (pNum == 1) {

    //
    //  Exactly one match for this SNP
    //

    if (numMulti == 0) {

      //  Match has one exon

      if (singleSingleFile)
        singleSingleFile->writeAlignment(p[0]);

      if (printSNP(validSNPMapFile, p[0])) {
        sspass++;
      } else {
        ssfail++;
        if (failedSNPMapFile)
          failedSNPMapFile->writeAlignment(p[0]);
      }
    } else {

      //  Match has more than one exon

      if (singleMultiFile)
        singleMultiFile->writeAlignment(p[0]);

      if (printSNP(validSNPMapFile, p[0])) {
        smpass++;
      } else {
        smfail++;
        if (failedSNPMapFile)
          failedSNPMapFile->writeAlignment(p[0]);
      }
    }
  } else {

    //
    //  More than one match for this SNP
    //

    if (numMulti == 0) {
      int pass=0, fail=0;

      //  All the matches are single exon

      if (multiSingleFile)
        for (i=0; i<pNum; i++)
          multiSingleFile->writeAlignment(p[i]);

      for (i=0; i<pNum; i++)
        if (printSNP(validSNPMapFile, p[i])) {
          pass++;
        } else {
          fail++;
          if (failedSNPMapFile)
            failedSNPMapFile->writeAlignment(p[i]);
        }

      if (pass==1)       sspass++;
      if (pass > 1)      mspass++;
      if (!pass && fail) msfail++;
    } else {
      int pass=0, fail=0;

      //  At least one match has more than one exon -- the correct one
      //  might be a single exon, but we don't know which is which.

      if (multiMultiFile)
        for (i=0; i<pNum; i++)
          multiMultiFile->writeAlignment(p[i]);

      for (i=0; i<pNum; i++)
        if (printSNP(validSNPMapFile, p[i])) {
          pass++;
        } else {
          fail++;
          if (failedSNPMapFile)
            failedSNPMapFile->writeAlignment(p[i]);
        }

      if (pass==1)       smpass++;
      if (pass > 1)      mmpass++;
      if (!pass && fail) mmfail++;
    }
  }

  for (i=0; i<pNum; i++)
    delete p[i];
}


int
main(int argc, char **argv) {
  int          pNum   = 0;
  int          pAlloc = 8388608;
  uint32       estID  = 0;

  uint32       percentID = 0;
  uint32       percentCO = 0;

  validSNPMapFile   = 0L;
  failedSNPMapFile  = 0L;

  int arg = 1;
  int err = 0;
  while (arg < argc) {
    if        (strncmp(argv[arg], "-i", 2) == 0) {
      percentID = atoi(argv[++arg]);

    } else if (strncmp(argv[arg], "-c", 2) == 0) {
      percentCO = atoi(argv[++arg]);

    } else if (strncmp(argv[arg], "-F", 2) == 0) {
      failedSNPMapFile = new sim4polishWriter(argv[++arg], sim4polishS4DB);

    } else if (strncmp(argv[arg], "-O", 2) == 0) {
      errno = 0;
      validSNPMapFile = fopen(argv[++arg], "w");
      if (errno)
        fprintf(stderr, "Couldn't open '%s' for writing: %s\n", argv[arg], strerror(errno)), exit(1);

    } else if (strncmp(argv[arg], "-D", 2) == 0) {
      char   name[FILENAME_MAX];

      arg++;

      sprintf(name, "%s-multi-multi", argv[arg]);
      multiMultiFile = new sim4polishWriter(name, sim4polishS4DB);

      sprintf(name, "%s-multi-single", argv[arg]);
      multiSingleFile = new sim4polishWriter(name, sim4polishS4DB);

      sprintf(name, "%s-single-multi", argv[arg]);
      singleMultiFile = new sim4polishWriter(name, sim4polishS4DB);

      sprintf(name, "%s-single-single", argv[arg]);
      singleSingleFile = new sim4polishWriter(name, sim4polishS4DB);

    } else if (strncmp(argv[arg], "-d", 2) == 0) {
      fieldDelimiter = argv[++arg][0];

    } else if (strncmp(argv[arg], "-p", 2) == 0) {
      posTag = argv[++arg];

    } else if (strncmp(argv[arg], "-s", 2) == 0) {
      sizeTag = argv[++arg];

    } else if (strncmp(argv[arg], "-o", 2) == 0) {
      positionOffset = atoi(argv[++arg]);

    } else if (strncmp(argv[arg], "-format", 2) == 0) {
      outputFormat = atoi(argv[++arg]);

    } else {
      fprintf(stderr, "unknown option: %s\n", argv[arg]);
      err++;
    }

    arg++;
  }

  if (err) {
    fprintf(stderr, "usage: %s [options]\n", argv[0]);
    fprintf(stderr, "             -i min-identity     filter matches on percent identity\n");
    fprintf(stderr, "             -c min-coverage     filter matches on percent coverage\n");
    fprintf(stderr, "             -F failed           save matches that do not contain the\n");
    fprintf(stderr, "                                 to the file 'failed'\n");
    fprintf(stderr, "             -O output           save the parsed SNPs to the file\n");
    fprintf(stderr, "                                 'output'\n");
    fprintf(stderr, "             -D prefix           report debugging stuff into files\n");
    fprintf(stderr, "                                 prefixed with 'prefix'\n");
    fprintf(stderr, "             -d delimiter        Use the single character delimiter as\n");
    fprintf(stderr, "                                 the end of the defline ID field.  The\n");
    fprintf(stderr, "                                 default is to split on any whitespace.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "             -s sizeTag          Use this tag as the size of the snp.\n");
    fprintf(stderr, "                                 '/size=' is tried by default.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "             -p posTag           Use this tag as the position of the snp.\n");
    fprintf(stderr, "                                 'allelePos=' and '/pos=' are tried by\n");
    fprintf(stderr, "                                 default, and if posTag is not found.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "                                 TAGS: The number immediately after the first\n");
    fprintf(stderr, "                                 occurance of the tag will be used.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "             -o offset           An additive offset to the SNP position.\n");
    fprintf(stderr, "                                 The default is 0.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "             -format n           1 - use the original (default) format\n");
    fprintf(stderr, "                                 2 - use an extended format, includes the\n");
    fprintf(stderr, "                                     position in the alignment string\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "             -h                  Show this help.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\n" );
    fprintf(stderr, "             only -O is required.  Input is read from stdin.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "             NOTE!  Sizes and sizeTag is NOT IMPLEMENTED!\n");
    fprintf(stderr, "                    All SNPs are of size == 1\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "             If you parse base-based SNPs, the result is returned base-based.\n");
    fprintf(stderr, "             You should use an ofset of 0.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "             If you parse space-based SNPs, the result is returned base-based.\n");
    fprintf(stderr, "             You should use an offset of 1.\n");
    fprintf(stderr, "\n");
  }

  if ((outputFormat != 1) && (outputFormat != 2)) {
    fprintf(stderr, "Invalid output format.  Must be 1 or 2.\n");
    exit(1);
  }


  //  Read polishes, parsing when we see a change in the estID.
  //  Really, we could parse one by one, but it's nice to know if the
  //  thing mapped more than once.
  //
  //  We could also extend this to discard matches that look
  //  suspicious -- or maybe pick the single best match for each.

  sim4polishReader  *R = new sim4polishReader("-");
  sim4polish       **p = new sim4polish * [pAlloc];
  sim4polish        *q = 0L;

  while (R->nextAlignment(q)) {
    if (q->_estID < estID) {
      fprintf(stderr, "ERROR:  Polishes not sorted by SNP idx!  this="uint32FMT", looking for "uint32FMT"\n",
              q->_estID, estID);
      exit(1);
    }

    if ((q->_estID != estID) && (pNum > 0)) {
      parseSNP(p, pNum);
      pNum  = 0;
    }

    if (pNum >= pAlloc) {
      sim4polish **P = new sim4polish * [pAlloc * 2];
      memcpy(p, P, sizeof(sim4polish *) * pAlloc);
      delete [] p;
      p = P;
      pAlloc *= 2;
    }

    estID     = q->_estID;

    if ((q->_percentIdentity >= percentID) &&
        (q->_querySeqIdentity >= percentCO)) {
      p[pNum++] = q;
    } else {
      delete q;
    }

    q = 0L;  //  Otherwise we delete the one we just saved!
  }

  if (pNum > 0)
    parseSNP(p, pNum);

  fprintf(stdout, "SNPs with:\n");
  fprintf(stdout, "  single hit, single exon:        %6d\n", sspass);
  fprintf(stdout, "  single hit, multiple exons:     %6d\n", smpass);
  fprintf(stdout, "  multiple hits, single exon:     %6d\n", mspass);
  fprintf(stdout, "  multiple hits, multiple exons:  %6d\n", mmpass);
  fprintf(stdout, "SNPs that failed:\n");
  fprintf(stdout, "  single hit, single exon:        %6d\n", ssfail);
  fprintf(stdout, "  single hit, multiple exons:     %6d\n", smfail);
  fprintf(stdout, "  multiple hits, single exon:     %6d\n", msfail);
  fprintf(stdout, "  multiple hits, multiple exons:  %6d\n", mmfail);

  fclose(validSNPMapFile);
  delete failedSNPMapFile;

  delete multiMultiFile;
  delete multiSingleFile;
  delete singleMultiFile;
  delete singleSingleFile;

  return(0);
}

