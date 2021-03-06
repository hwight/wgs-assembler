#include "hitMatrix.H"

//  Sort by dsPos

inline
void
adjustHeap_dsPos(diagonalLine *L, uint32 p, uint32 n) {
  uint32  q = L[p]._qsPos;
  uint32  d = L[p]._dsPos;
#ifndef WITHOUT_DIAGONALID
  uint32  l = L[p]._diagonalID;
#endif
  uint32  c = (p << 1) + 1;  //  let c be the left child of p

  while (c < n) {

    //  Find the larger of the two children
    //
    if ((c+1 < n) && (L[c]._dsPos < L[c+1]._dsPos))
      c++;

    //  Does the node in question fit here?
    //
    if (d >= L[c]._dsPos)
      break;

    //  Else, swap the parent and the child
    //
    L[p]._qsPos      = L[c]._qsPos;
    L[p]._dsPos      = L[c]._dsPos;
#ifndef WITHOUT_DIAGONALID
    L[p]._diagonalID = L[c]._diagonalID;
#endif

    //  Move down the tree
    //
    p = c;
    c = (p << 1) + 1;
  }

  L[p]._qsPos      = q;
  L[p]._dsPos      = d;
#ifndef WITHOUT_DIAGONALID
  L[p]._diagonalID = l;
#endif
}

void
hitMatrix::sort_dsPos(void) {

  if (_hitsLen > 1) {

    //  Create the heap of lines.
    //
    for (uint32 i=_hitsLen/2; i--; )
      adjustHeap_dsPos(_hits, i, _hitsLen);

    //  Interchange the new maximum with the element at the end of the tree
    //
    for (uint32 i=_hitsLen-1; i>0; i--) {
      uint32  q  = _hits[i]._qsPos;
      uint32  d  = _hits[i]._dsPos;
#ifndef WITHOUT_DIAGONALID
      uint32  l  = _hits[i]._diagonalID;
#endif

      _hits[i]._qsPos      = _hits[0]._qsPos;
      _hits[i]._dsPos      = _hits[0]._dsPos;
#ifndef WITHOUT_DIAGONALID
      _hits[i]._diagonalID = _hits[0]._diagonalID;
#endif

      _hits[0]._qsPos      = q;
      _hits[0]._dsPos      = d;
#ifndef WITHOUT_DIAGONALID
      _hits[0]._diagonalID = l;
#endif
      
      adjustHeap_dsPos(_hits, 0, i);
    }
  }
}
