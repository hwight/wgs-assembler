#include "snapper2.H"



uint32
configureFilter(double L,
                double H,
                double V,
                aHit  *theHits,
                uint32 theHitsLen) {

  //  Find the highest and lowest quality hit
  //
  uint32  hiQ = theHits[0]._covered;
  uint32  loQ = hiQ;

  for (uint32 i=0; i<theHitsLen; i++) {
    if (hiQ < theHits[i]._covered)
      hiQ = theHits[i]._covered;
    if (loQ > theHits[i]._covered)
      loQ = theHits[i]._covered;
  }

  //  _numMers is not the same as the number covered, so we should
  //  ensure that h is in range.
  //
  //  Note: _numMers is constant for all hits, so we can use any of them
  //
  double h = (double)(hiQ - loQ) / (double)theHits[0]._numMers;
  if (h > 1.0)
    h = 1.0;

  double p = 0.0;
  if      (h <= L)  p = 1.0;
  else if (h >= H)  p = V;
  else              p = 1.0 - (1.0 - V) * (h - L) / (H - L);

  if (p > 1.0) {
    fprintf(stderr, "error in p; p=%f > 1.0!  h=%f (L=%f H=%f V=%f)\n", p, h, L, H, V);
    p = 1.0;
  }

  if (V - p > 1e-10) {
    fprintf(stderr, "error in p; p=%f < V!  h=%f (L=%f H=%f V=%f)\n", p, h, L, H, V);
    p = V;
  }

  //  Any thing at or above cutL is good, and we should polish it.
  //  Anything below is junk, and we should ignore it.
  //
  return((uint32)floor(hiQ - p * h * theHits[0]._numMers));
}



int
aHitAutoFilterSort(const void *a, const void *b) {
  const aHit  *A = (const aHit *)a;
  const aHit  *B = (const aHit *)b;

  //  If either was discarded, we don't care the order,
  //  just throw them at the end of the array
  //
  if ((A->_status & AHIT_DISCARDED) ||
      (B->_status & AHIT_DISCARDED)) {
    if      (A->_status & AHIT_DISCARDED)
      return(1);
    else if (B->_status & AHIT_DISCARDED)
      return(-1);
    return(0);
  }

  //  Otherwise, snapper filters simply on coverage.
  //
  if      (A->_covered > B->_covered)
    return(-1);
  else if (A->_covered < B->_covered)
    return(1);
  return(0);
}



void
doFilter(searcherState       *state,
         query               *qry) {

  if (qry->theHitsLen == 0)
    return;

  uint32 numF = 0;

  //  Auto filter -- keep polishing until a running average of
  //  polishes falls below some threshold.
  //
  if (config._afEnabled) {
    qsort(qry->theHits, qry->theHitsLen, sizeof(aHit), aHitAutoFilterSort);

    for (uint32 i=0; i < qry->theHitsLen; i++)
      qry->theHits[i]._status |= AHIT_POLISHABLE;

    numF = qry->theHitsLen;

  } else {
    uint32 cutL = configureFilter(config._Lo,
                                  config._Hi,
                                  config._Va, qry->theHits, qry->theHitsLen);

    //  If the coverage of the hit is more than the minimum, mark the
    //  hit as polishable.  Unless the hit was discarded.

    for (uint32 i=0; i < qry->theHitsLen; i++) {
      if (!(qry->theHits[i]._status & AHIT_DISCARDED) &&
          (qry->theHits[i]._covered >= cutL)) {
        qry->theHits[i]._status |= AHIT_POLISHABLE;
        numF++;
      }
    }
  }

#ifdef VERBOSE_FILTER
  if (qry->theHitsLen >= VERBOSE_FILTER_MINIMUM)
    theLog->add("Query "uint32FMT" with "uint32FMT" good hits out of "uint32FMT" total hits.\n",
                idx, numF, qry->theHitsLen);
#endif
}
