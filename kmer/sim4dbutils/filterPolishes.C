#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "bio.h"
#include "sim4.H"

int
main(int argc, char ** argv) {
  uint32              minC = 0;
  uint32              minI = 0;
  uint32              minL = 0;
  uint32              cdna = ~uint32ZERO;
  uint32              geno = ~uint32ZERO;
  uint32              minExons = 0;
  uint32              maxExons = ~uint32ZERO;
  uint32              beVerbose = 0;
  int                 GOODsilent = 0;
  sim4polishWriter   *GOOD       = 0L;
  int                 CRAPsilent = 0;
  sim4polishWriter   *CRAP       = 0L;
  sim4polishWriter   *JUNK = 0L;
  uint64              pmod = 1;
  uint64              good = 0;
  uint64              crap = 0;
  uint64              junk = 0;
  int                 doSelfFilter = 0;
  int                 doSegregation = 0;
  uint32              doSegregationLo = 0;
  uint32              doSegregationHi = 0;
  char               *filePrefixGOOD = 0L;
  char               *filePrefixCRAP = 0L;
  char               *filePrefixJUNK = 0L;
  sim4polishWriter  **SEGREGATE = 0L;
  bool                noDefLines = false;
  bool                noAlignments = false;
  bool                doGFF3 = false;
  sim4polishStyle     style = sim4polishStyleDefault;

  //  We limit scaffolds to be below the number of open files per
  //  process.
  //
  uint32       maxScaffold = sysconf(_SC_OPEN_MAX);

  int arg = 1;
  while (arg < argc) {
    if        (strncmp(argv[arg], "-verbose", 2) == 0) {
      beVerbose = 1;

    } else if (strncmp(argv[arg], "-c", 2) == 0) {
      minC = atoi(argv[++arg]);

    } else if (strncmp(argv[arg], "-i", 2) == 0) {
      minI = atoi(argv[++arg]);

    } else if (strncmp(argv[arg], "-l", 2) == 0) {
      minL = atoi(argv[++arg]);

    } else if (strncmp(argv[arg], "-minexons", 3) == 0) {
      minExons = atoi(argv[++arg]);

    } else if (strncmp(argv[arg], "-maxexons", 3) == 0) {
      maxExons = atoi(argv[++arg]);

    } else if (strncmp(argv[arg], "-o", 2) == 0) {
      filePrefixGOOD = argv[++arg];
      GOODsilent = 0;

    } else if (strncmp(argv[arg], "-O", 2) == 0) {
      GOODsilent = 1;

    } else if (strncmp(argv[arg], "-d", 2) == 0) {
      filePrefixCRAP = argv[++arg];
      CRAPsilent = 0;

    } else if (strncmp(argv[arg], "-q", 2) == 0) {
      CRAPsilent = 1;

    } else if (strncmp(argv[arg], "-D", 2) == 0) {
      CRAPsilent = 1;

    } else if (strncmp(argv[arg], "-j", 2) == 0) {
      filePrefixJUNK = argv[++arg];

    } else if (strncmp(argv[arg], "-C", 2) == 0) {
      cdna = atoi(argv[++arg]);

    } else if (strncmp(argv[arg], "-G", 2) == 0) {
      geno = atoi(argv[++arg]);

    } else if (strncmp(argv[arg], "-selfhits", 4) == 0) {
      doSelfFilter = 1;

    } else if (strncmp(argv[arg], "-segregate", 4) == 0) {
      doSegregation = 1;
      doSegregationLo = atoi(argv[++arg]);
      doSegregationHi = atoi(argv[++arg]);
      if (doSegregationHi - doSegregationLo + 1 > maxScaffold)
        fprintf(stderr, "error: -segregate range too big; must be less than %u.\n", maxScaffold), exit(1);
      SEGREGATE = new sim4polishWriter * [maxScaffold];
      memset(SEGREGATE, 0, sizeof(sim4polishWriter *) * maxScaffold);

    } else if (strncmp(argv[arg], "-nodeflines", 4) == 0) {
      noDefLines = true;

    } else if (strncmp(argv[arg], "-noalignments", 4) == 0) {
      noAlignments = true;

    } else if (strncmp(argv[arg], "-gff3", 4) == 0) {
      doGFF3 = true;
      style = sim4polishGFF3;

    } else {
      fprintf(stderr, "UNKNOWN option '%s'\n", argv[arg]);
      exit(1);
    }

    arg++;
  }

  if (isatty(fileno(stdin))) {
    fprintf(stderr, "usage: %s [-c c] [-i i] [-o o]\n", argv[0]);
    fprintf(stderr, "  -verbose       Report progress\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -c c           Discard polishes below c%% composite (default: 0).\n");
    fprintf(stderr, "  -i i           Discard polishes below i%% identity (default: 0).\n");
    fprintf(stderr, "  -l l           Discard polishes below l identities (default: 0).\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -minexons e    Discard polishes below e exons (default: 0).\n");
    fprintf(stderr, "  -maxexons e    Discard polishes above e exons (default: infinity).\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -C c           Discard polishes that are not from cDNA idx 'c'\n");
    fprintf(stderr, "  -G g           Discard polishes that are not from genomic idx 'g'\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -o o           Write saved polishes to the 'o' file (default == stdout).\n");
    fprintf(stderr, "  -O             Don't write saved polishes.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -d o           Write discarded polishes to the 'o' file (default == stdout).\n");
    fprintf(stderr, "  -D             Don't write discarded polishes.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -j o           Write intractable and aborted polishes to the 'o' file.  By\n");
    fprintf(stderr, "                 default these are silently discarded.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -selfhits      Filter out alignments to ourself -- if you did an all-to-all\n");
    fprintf(stderr, "                 mapping of a set onto itself.  Deflines needed!\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -segregate a b Segregate polishes by genomic idx, for idx's between a and b inclusive.\n");
    fprintf(stderr, "                 b-a must be less than %u.\n", maxScaffold);
    fprintf(stderr, "                 Must be used with -o.\n");
    fprintf(stderr, "                 Will create numerous files 'o.%%05d'.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -nodeflines    Strip out deflines.\n");
    fprintf(stderr, "  -noalignments  Strip out alignments.\n");
    fprintf(stderr, "  -gff3          Write output in GFF3 format.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "                 All conditions must be met.\n");
    exit(1);
  }

  if ((CRAPsilent == 0) && (GOODsilent == 0) && (filePrefixGOOD == 0L) && (filePrefixCRAP == 0L)) {
    fprintf(stderr, "error: filter has no effect; saved and discarded polishes\n");
    fprintf(stderr, "       both printed to the same place!\n");
    fprintf(stderr, "       (try using one of -o, -O, -d, -D)\n");
    exit(1);
  }

  if (doSegregation && (filePrefixGOOD == 0L)) {
    fprintf(stderr, "error: you must specify a file prefix when segregating (-s requires -o)\n");
    exit(1);
  }

  if (noDefLines && doGFF3)
    fprintf(stderr, "warning: No deflines option inactive with GFF3.\n");

  if (beVerbose) {
    fprintf(stderr, "Filtering at "uint32FMT"%% coverage and "uint32FMT"%% identity and "uint32FMT"bp.\n", minC, minI, minL);

    if ((cdna != ~uint32ZERO) && (cdna != ~uint32ZERO))
      fprintf(stderr, "Filtering for cDNA idx "uint32FMT" and genomic idx "uint32FMT"\n", cdna, geno);
    else if (cdna != ~uint32ZERO)
      fprintf(stderr, "Filtering for cDNA idx "uint32FMT".\n", cdna);
    else if (geno != ~uint32ZERO)
      fprintf(stderr, "Filtering for genomic idx "uint32FMT".\n", geno);
  }

  //  Prepare input files

  sim4polishReader *R = new sim4polishReader("-");
  sim4polish       *p = 0L;

  if (R->getsim4polishStyle() != style)
    fprintf(stderr, "warning: input format and output format differ.\n");

  // Prepare output files 

  if (filePrefixGOOD != 0L) 
    GOOD = new sim4polishWriter(filePrefixGOOD, style);
  if (filePrefixCRAP != 0L)
    CRAP = new sim4polishWriter(filePrefixCRAP, style); 
  if (filePrefixJUNK != 0L)
    JUNK = new sim4polishWriter(filePrefixJUNK, style);

  if ((CRAPsilent == 0) && (CRAP == 0L))
    CRAP = new sim4polishWriter("-", sim4polishS4DB);

  if ((GOODsilent == 0) && (GOOD == 0L))
    GOOD = new sim4polishWriter("-", sim4polishS4DB);


  //  Start processing

  while (R->nextAlignment(p)) {

    if (noDefLines && (doGFF3 == false))
      p->s4p_removeDefLines();
    if (noAlignments)
      p->s4p_removeAlignments();

    if (JUNK && ((p->_strandOrientation == SIM4_STRAND_INTRACTABLE) ||
                 (p->_strandOrientation == SIM4_STRAND_FAILED))) {
      junk++;
      JUNK->writeAlignment(p);
    } else {
      if ((p->_percentIdentity  >= minI) &&
          (p->_querySeqIdentity >= minC) &&
          (p->_numCovered  >= minL) &&
          ((cdna == ~uint32ZERO) || (cdna == p->_estID)) &&
          ((geno == ~uint32ZERO) || (geno == p->_genID)) &&
          (minExons <= p->_numExons) &&
          (p->_numExons <= maxExons) &&
          ((doSelfFilter == 0) || (strcmp(p->_estDefLine, p->_genDefLine) != 0))) {
        good++;
        if (doSegregation) {
          if ((doSegregationLo <= p->_genID) &&
              (p->_genID <= doSegregationHi)) {
            if (SEGREGATE[p->_genID - doSegregationLo] == 0L) {
              char filename[1024];
              sprintf(filename, "%s.%04d", filePrefixGOOD, (int)p->_genID);
              SEGREGATE[p->_genID - doSegregationLo] = new sim4polishWriter(filename, sim4polishS4DB);
            }
            SEGREGATE[p->_genID - doSegregationLo]->writeAlignment(p);
          }
        } else {
          if (!GOODsilent)
            GOOD->writeAlignment(p);
        }
      } else {
        crap++;
        if (!CRAPsilent)
          CRAP->writeAlignment(p);
      }
    }

    if ((beVerbose) && ((good+crap) == pmod)) {
      pmod += 8888 + (random() % 1000);
      if (junk > 0)
        fprintf(stderr, " Filter: %6.2f%% ("uint64FMT" matches processed) ("uint64FMT" failed/intractable)\r",
                100.0 * good / (good+crap),
                good+crap,
                junk);
      else
        fprintf(stderr, " Filter: %6.2f%% ("uint64FMT" matches processed)\r",
                100.0 * good / (good+crap),
                good+crap);
      fflush(stderr);
    }
  }


  if (beVerbose) {
    if (junk > 0)
      fprintf(stderr, " Filter: %6.2f%% ("uint64FMT" matches processed) ("uint64FMT" failed/intractable)\n",
              100.0 * good / (good+crap),
              good+crap,
              junk);
    else
      fprintf(stderr, " Filter: %6.2f%% ("uint64FMT" matches processed)\n",
              100.0 * good / (good+crap),
              good+crap);
  }

  delete R;

  if (doSegregation) {
    for (uint32 i=0; i<maxScaffold; i++)
      if (SEGREGATE[i])
        delete SEGREGATE[i];
    delete [] SEGREGATE;
  }

  delete GOOD;
  delete JUNK;

  return(0);
}
