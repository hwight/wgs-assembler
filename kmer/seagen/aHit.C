#include "aHit.H"

#include <stdio.h>
#include <stdlib.h>


void   ahit_writeBinary(aHit *a, FILE *F) {
  fwrite(a, sizeof(aHit), 1, F);
}

void   ahit_readBinary(aHit *a, FILE *F) {
  fread(a, sizeof(aHit), 1, F);
}

void   ahit_readBinary(aHit *a, readBuffer *F) {
  F->read((char *)a, sizeof(aHit));
}

void   ahit_printASCII(aHit *a, FILE *F) {
  fprintf(F, "-%c -e "uint32FMT" -D "uint32FMT" "uint32FMT" "uint32FMT" -M "uint32FMT" "uint32FMT" "uint32FMT"\n",
          a->_forward ? 'f' : 'r',
          a->_qsIdx,
          a->_dsIdx,
          a->_dsLo,
          a->_dsHi,
          a->_covered,
          a->_matched,
          a->_numMers);
}


//  We don't read the string here so that we can use a static buffer
//  in whatever loop we read with.
//  
//  e.g.,
//
//  char    b[1025];
//  while (!feof(I)) {
//    fgets(b, 1024, I);
//    if (!feof(I))
//      ahit_parseString(a, b);
//  }
//
//  Note that using sscanf, while easy to implement, and safe
//  (looking, anyways), is terribly slow, and not really that safe.
//
//  char    c;
//  sscanf(b, "-%c -e %d -D %d %d %d -M %d %d %d",
//         &c,
//         &a->_qsIdx,
//         &a->_dsIdx,
//         &a->_dsLo,
//         &a->_dsHi,
//         &a->_covered,
//         &a->_matched,
//         &a->_numMers);
//  a->_direction = (c == 'f');
//
//  fast: 138.440u 40.500s  4:04.61 73.1% 0+2k 327822+0io  4pf+0w
//  slow: 737.587u 38.652s 13:12.42 97.9% 0+2k 328006+0io 11pf+0w
//
void   ahit_parseString(aHit *a, char *b) {
  char *c = b+1;

  a->_forward = (*c == 'f');

  c += 1;

  if (c[2] != 'e')  fprintf(stderr, "'%s' didn't get -e\n", b);

  c += 4;
  a->_qsIdx     = (uint32)strtoul(c, &c, 10);

  //  If we get a "-D" next then we are reading search output,
  //  otherwise, we are (hopefully) reading seatac output.

  if (c[2] == 'D') {

    //  searchGENOME format here!

    c += 4;
    a->_dsIdx     = (uint32)strtoul(c, &c, 10);
    a->_dsLo      = (uint32)strtoul(c, &c, 10);
    a->_dsHi      = (uint32)strtoul(c, &c, 10);

    if (c[2] == 'M') {
      c += 4;
      a->_covered   = (uint32)strtoul(c, &c, 10);
      a->_matched   = (uint32)strtoul(c, &c, 10);
      a->_numMers   = (uint32)strtoul(c, &c, 10);
    } else {
      a->_covered   = 0;
      a->_matched   = 0;
      a->_numMers   = 0;
    }
  } else {

    //  seatac format here!
#if 0
    fprintf(stderr, "seatac?\n");


    //  We make horrible use of variable names here -- covered and
    //  matched are the regions on the first sequence, and numMers
    //  is the "F" value.

    a->_covered   = (uint32)strtoul(c, &c, 10);
    a->_matched   = (uint32)strtoul(c, &c, 10);

    c += 4;
    a->_dsIdx     = (uint32)strtoul(c, &c, 10);
    a->_dsLo      = (uint32)strtoul(c, &c, 10);
    a->_dsHi      = (uint32)strtoul(c, &c, 10);

    c += 4;
    a->_numMers   = (uint32)strtoul(c, &c, 10);
#endif
  }
}
