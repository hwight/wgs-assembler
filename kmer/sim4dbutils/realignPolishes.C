#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "bio++.H"
#include "seqCache.H"
#include "sim4.H"

//  This code takes basic sim4db format polishes and recomputes the
//  alignments and scores.  Required in the input polishes are the EST
//  id, genomic id, exon coordinates and an orientation.

int
main(int argc, char **argv) {

  //  Load all the sequences.  We really do need all the ESTs in core,
  //  since they probably aren't in a useful sorted order.  You can
  //  probably figure out a way to get rid of the seqCache for the
  //  GEN.  Doing so will reduce memory usage by about 50%.

  seqCache   *EST = 0L;
  seqCache   *GEN = 0L;
  int         mergeTolerancePerc = 0;
  int         mergeToleranceBase = 0;
  int         statsOnly          = 0;
  int         warnOnChange       = 0;

  //  Statistics on the exon merge

  int   mergedExons   = 0;
  int   mergedMatches = 0;

  int   numcdnagaps        = 0;
  int   nummatcheswithgaps = 0;

  FILE *mergeLog      = 0L;

  int     arg = 1;
  while (arg < argc) {
    if        (strncmp(argv[arg], "-merge", 2) == 0) {
      mergeTolerancePerc = atoi(argv[++arg]);
    } else if (strncmp(argv[arg], "-b", 2) == 0) {
      mergeToleranceBase = atoi(argv[++arg]);
    } else if (strncmp(argv[arg], "-M", 2) == 0) {
      mergeLog = fopen(argv[++arg], "w");
    } else if (strncmp(argv[arg], "-e", 2) == 0) {
      if (statsOnly) {
        EST = new seqCache(argv[++arg], 1000, false);  //  debugging only!
      } else {
        EST = new seqCache(argv[++arg],    0, false);
        EST->loadAllSequences();
      }
    } else if (strncmp(argv[arg], "-g", 2) == 0) {
      GEN = new seqCache(argv[++arg],    0, false);
      GEN->loadAllSequences();
    } else if (strncmp(argv[arg], "-q", 2) == 0) {
      statsOnly = 1;
    } else if (strncmp(argv[arg], "-w", 2) == 0) {
      warnOnChange = 1;
    }
    arg++;
  }

  if ((statsOnly == 0) && (!EST || !GEN)) {
    fprintf(stderr, "usage: %s [-merge percent-tolerance] [-M merge-log] [-q] -e est.fasta -g genome.fasta < polishes > somewhere\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "       Polishes _MUST_ be sorted by genomic index.\n");
    fprintf(stderr, "       If not, performance will be worse than atrocious.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "       percent-tolerance -- merge exons separated by gap if\n");
    fprintf(stderr, "       the cDNA and genomic gaps differ by less than p percent.\n");
    fprintf(stderr, "       A value of 5 means 5%%\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "      -q: Don't actually do the work, just count the statistics\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\n");
    exit(1);
  }


  char   *s1 = new char [16 * 1024 * 1024];
  char   *s2 = new char [16 * 1024 * 1024];
  int     l1 = 0;
  int     l2 = 0;

  speedCounter  *C = new speedCounter("%12.0f polishes -- %12.0f polishes/second\r",
                                      1.0, 0xff, true);

  sim4polishWriter *W = new sim4polishWriter("-", sim4polishS4DB);
  sim4polishReader *R = new sim4polishReader("-");
  sim4polish       *p = 0L;

  while (R->nextAlignment(p)) {

    //fprintf(stdout, "BEFORE\n");
    //p->s4p_printPolish(stdout);

    //  If we have a mergeTolerance, merge adjacent exons that are
    //  separated my approximately equal sized cDNA and genomic gaps.
    //
    //  Possible a better way to do this is to check if the identity
    //  of the missing region is decent, too.

    //  Remember the id/cv of this guy for the log
    //
    double id = 0.0;
    double cv = 0.0;
    if (mergeLog) {
      id = p->s4p_percentIdentityExact();
      cv = p->s4p_percentCoverageExact();
    }

    int   merged = 0;
    int   gapped = 0;

    if ((mergeTolerancePerc > 0) || (mergeToleranceBase > 0)) {
      for (uint32 i=1; i<p->_numExons; i++) {
        int cgap = p->_exons[i]._estFrom - p->_exons[i-1]._estTo;
        int ggap = p->_exons[i]._genFrom - p->_exons[i-1]._genTo;

        bool  mergeGap = false;

        //  New method -- check if the gaps are within 20bp of each other
        //
        int diff = cgap - ggap;
        if (diff < 0)
          diff = -diff;

        if (diff < mergeToleranceBase)
          mergeGap = true;


        //  Original method -- cehck if the gaps are within 10% of each other
        //
        int ctol = cgap * (100 + mergeTolerancePerc);
        int gtol = ggap * (100 + mergeTolerancePerc);

        cgap *= 100;
        ggap *= 100;

        if (((cgap < ggap) && (ctol > ggap)) ||
            ((ggap < cgap) && (gtol > cgap)))
          mergeGap = true;

        if (cgap > 1) {
          numcdnagaps++;
          gapped++;
        }

        if ((cgap > 1) && (mergeGap)) {

          //  Merge i and i-1 if adding in the tolerance makes either
          //  the cgap or the ggap longer than the other gap.  i.e., the
          //  cgap was shorter, but including the tolerance makes it
          //  longer, so they're about the same size.

          if (mergeLog)
            fprintf(mergeLog,
                    "MERGE: "uint32FMTW(4)"-"uint32FMTW(4)" (%6.2f,%6.2f) "uint32FMTW(4)"-"uint32FMTW(4)
                    " and "uint32FMTW(8)"-"uint32FMTW(8)" (%6.2f,%6.2f) "uint32FMTW(8)"-"uint32FMTW(8)"\n",
                    p->_exons[i-1]._estFrom, p->_exons[i-1]._estTo,
                    cgap / 100.0, ctol / 100.0,
                    p->_exons[i]._estFrom, p->_exons[i]._estTo,
                    p->_exons[i-1]._genFrom, p->_exons[i-1]._genTo,
                    ggap / 100.0, gtol / 100.0,
                    p->_exons[i]._genFrom, p->_exons[i]._genTo);

          //  merge exons
          p->_exons[i-1]._estTo = p->_exons[i]._estTo;
          p->_exons[i-1]._genTo = p->_exons[i]._genTo;

          //  delete this exon
          p->s4p_deleteExon(i);

          //  Do it again!
          i--;

          merged++;
          mergedExons++;
        }
      }

      if (merged)
        mergedMatches++;
      if (gapped)
        nummatcheswithgaps++;
    }


    //  For each exon, generate an alignment


    if (statsOnly == 0) {
      p->_estLen   = EST->getSequenceInCore(p->_estID)->sequenceLength();
      p->_estPolyA = 0;
      p->_estPolyT = 0;

      for (uint32 i=0; i<p->_numExons; i++) {
        l1 = p->_exons[i]._estTo - p->_exons[i]._estFrom + 1;
        l2 = p->_exons[i]._genTo - p->_exons[i]._genFrom + 1;

        strncpy(s1, EST->getSequenceInCore(p->_estID)->sequence() + p->_exons[i]._estFrom - 1, l1);
        strncpy(s2, GEN->getSequenceInCore(p->_genID)->sequence() + p->_exons[i]._genFrom - 1, l2);

        if (p->_matchOrientation == SIM4_MATCH_COMPLEMENT) {
          strncpy(s1, EST->getSequenceInCore(p->_estID)->sequence() + p->_estLen - p->_exons[i]._estTo, l1);
          reverseComplementSequence(s1, l1);
        }

        s1[l1] = 0;
        s2[l2] = 0;

        delete [] p->_exons[i]._estAlignment;
        delete [] p->_exons[i]._genAlignment;

        p->_exons[i]._estAlignment = new char [l1+l2+1];
        p->_exons[i]._genAlignment = new char [l1+l2+1];

        halign(s1, s2,
               l1, l2,
               p->_exons[i]._estAlignment,
               p->_exons[i]._genAlignment);
      }

      //  There isn't an intron after the last exon.  Force it.
      //
      p->_exons[p->_numExons-1]._intronOrientation = SIM4_INTRON_NONE;

      //  Check that we didn't radically change things
      uint32 nm = p->_numMatches;

      p->s4p_updateAlignmentScores();

      W->writeAlignment(p);

      if (warnOnChange) {
        uint32 diff = 0;
        if (nm < p->_numMatches)  diff = p->_numMatches - nm;
        if (nm > p->_numMatches)  diff = nm - p->_numMatches;

        if (diff > p->_numMatches / 100)
          fprintf(stdout, "WARNING: CHANGED! "uint32FMT" -> "uint32FMT"\n", nm, p->_numMatches);
      }
    }

    if (merged) {
      fprintf(mergeLog, "MERGED\tEST\t"uint32FMT"\tfrom\t%8.3f\t%8.3f\tto\t%8.3f\t%8.3f\n",
              p->_estID, id, cv, p->s4p_percentIdentityExact(), p->s4p_percentCoverageExact());
    }

    C->tick();
  }

  if ((mergeTolerancePerc > 0) || (mergeToleranceBase > 0)) {
    fprintf(stderr, "FOUND:  %d gaps in %d matches.\n", numcdnagaps, nummatcheswithgaps);
    fprintf(stderr, "MERGED: %d gaps in %d matches.\n", mergedExons, mergedMatches);
  }

  delete GEN;
  delete EST;

  return(0);
}
