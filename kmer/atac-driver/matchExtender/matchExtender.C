// This file is part of A2Amapper.
// Copyright (c) 2005 J. Craig Venter Institute
// Author: Brian Walenz
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received (LICENSE.txt) a copy of the GNU General Public 
// License along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <vector>
#include <algorithm>
using namespace std;

#include "atac.H"
#include "match.H"
#include "bio++.H"
#include "seqCache.H"


uint32  minEndRunLen = 10;    // -E /matchExtenderMinEndRunLen
uint32  maxMMBlock   = 3;     // -B /matchExtenderMaxMMBlock
uint32  minBlockSep  = 20;    // -S /matchExtenderMinBlockSep
double  minIdentity  = 0.95;  // -I /matchExtenderMinIdentity
uint32  maxNbrSep    = 100;   // -P /matchExtenderMaxNbrSep
uint32  maxNbrPathMM = 5;     // -D /matchExtenderMaxNbrPathMM


bool   trim_to_pct(vector<match_s *>& matches, uint32 midx, double pct);
void   extend_match_backward(vector<match_s *>& matches, uint32 midx, uint32 min_start_pos);
bool   can_reach_nearby_match(match_s *src, match_s *dest);
bool   extend_match_forward(vector<match_s *>& matches, uint32 midx, match_s *target);
uint32 extend_matches_on_diagonal(vector<match_s *>& matches, uint32 diag_start);


class MatchCompare {
public:
  int operator()(const match_s *m1, const match_s *m2) {
    return(*m1 < *m2);
  }
};



//  Read matches until the iid differs.  Leave the next match in inLine.
//
bool
readMatches(atacFileStreamMerge   &AF,
            atacMatch            *&m,
            seqCache              *C1,
            seqCache              *C2,
            vector<match_s *>     &fwdMatches,
            vector<match_s *>     &revMatches) {

  fwdMatches.clear();
  revMatches.clear();

  //  If M is null, we're here for the first time, so get the next
  //  (first) match from the file.  M is also null if we're at the end
  //  of the file, so if after getting a match (that's done at the end
  //  of this routine) we're still null, we're all done.
  //
  if (m == 0L)
    m = AF.nextMatch('x');
  if (m == 0L)
    return(false);

  uint32               iid1 = m->iid1;
  uint32               iid2 = m->iid2;

  seqInCore *seq1 = C1->getSequenceInCore(iid1);
  seqInCore *seq2 = C2->getSequenceInCore(iid2);

  while (m) {
    if ((m->iid1 == iid1) && (m->iid2 == iid2)) {

      if (m->fwd1 == m->fwd2)
        fwdMatches.push_back(new match_s(m->matchuid,
                                         seq1, m->iid1, m->pos1, m->len1, m->fwd1,
                                         seq2, m->iid2, m->pos2, m->len2, m->fwd2));
      else
        revMatches.push_back(new match_s(m->matchuid,
                                         seq1, m->iid1, m->pos1, m->len1, m->fwd1,
                                         seq2, m->iid2, m->pos2, m->len2, m->fwd2));
    } else {
      break;
    }

    m = AF.nextMatch('x');
  }

  if (fwdMatches.size() > 0)
    sort(fwdMatches.begin(), fwdMatches.end(), MatchCompare());

  if (revMatches.size() > 0)
    sort(revMatches.begin(), revMatches.end(), MatchCompare());

  return(true);
}




int
main(int argc, char *argv[]) {
  bool    fail = false;

  atacMatch            *m = 0L;
  atacFileStreamMerge   AF;

  int arg=1;
  while (arg < argc) {
    if        (strcmp(argv[arg], "-e") == 0) {
      minEndRunLen = strtouint32(argv[++arg], 0L);
    } else if (strcmp(argv[arg], "-b") == 0) {
      maxMMBlock   = strtouint32(argv[++arg], 0L);
    } else if (strcmp(argv[arg], "-s") == 0) {
      minBlockSep  = strtouint32(argv[++arg], 0L);
    } else if (strcmp(argv[arg], "-i") == 0) {
      minIdentity  = atof(argv[++arg]);
    } else if (strcmp(argv[arg], "-p") == 0) {
      maxNbrSep    = strtouint32(argv[++arg], 0L);
    } else if (strcmp(argv[arg], "-d") == 0) {
      maxNbrPathMM = strtouint32(argv[++arg], 0L);
    } else {
      //fprintf(stderr, "unknown option %s\n", argv[arg]);
      //fail = true;
      AF.addFile(argv[arg]);
    }

    arg++;
  }

  if (fail) {
    fprintf(stderr, "usage: %s [options] header.atac matches.atac ... > matches.atac\n", argv[0]);
    fprintf(stderr, "  -e <int>     matchExtenderMinEndRunLen,  10\n");
    fprintf(stderr, "  -b <int>     matchExtenderMaxMMBlock,     3\n");
    fprintf(stderr, "  -s <int>     matchExtenderMinBlockSep,   20\n");
    fprintf(stderr, "  -i <float>   matchExtenderMinIdentity,    0.95\n");
    fprintf(stderr, "  -p <int>     matchExtenderMaxNbrSep,    100\n");
    fprintf(stderr, "  -d <int>     matchExtenderMaxNbrPathMM,   5\n");
    exit(1);
  }

  AF.writeHeader(stdout);

  seqCache  *C1 = new seqCache(AF.assemblyFileA(), 1,  false);
  seqCache  *C2 = new seqCache(AF.assemblyFileB(), 1, false);

  C1->loadAllSequences();

  vector<match_s *>  fwdMatches;
  vector<match_s *>  revMatches;

  while (readMatches(AF, m, C1, C2, fwdMatches, revMatches)) {

    uint32 diag_start = 0;
    while (diag_start < fwdMatches.size()) {
      //fprintf(stderr, "fwd: M u %s . %s %d %d 1 %s %d %d 1\n",
      //        fwdMatches[diag_start]->_matchId,
      //        fwdMatches[diag_start]->_id1, fwdMatches[diag_start]->_acc1->getRangeBegin(), fwdMatches[diag_start]->_acc1->getRangeLength(),
      //        fwdMatches[diag_start]->_id2, fwdMatches[diag_start]->_acc2->getRangeBegin(), fwdMatches[diag_start]->_acc2->getRangeLength());
      diag_start = extend_matches_on_diagonal(fwdMatches, diag_start);
    }

    diag_start = 0;
    while (diag_start < revMatches.size()) {
      //fprintf(stderr, "rev: M u %s . %s %d %d 1 %s %d %d 1\n",
      //        revMatches[diag_start]->_matchId,
      //        revMatches[diag_start]->_id1, revMatches[diag_start]->_acc1->getRangeBegin(), revMatches[diag_start]->_acc1->getRangeLength(),
      //        revMatches[diag_start]->_id2, revMatches[diag_start]->_acc2->getRangeBegin(), revMatches[diag_start]->_acc2->getRangeLength());
      diag_start = extend_matches_on_diagonal(revMatches, diag_start);
    }


    //  Dump and destroy all the matches
    //
    for (uint32 i=0; i<fwdMatches.size(); i++) {
      if (!fwdMatches[i]->isDeleted())
        fprintf(stdout, "M u %s . %s:"uint32FMT" "uint32FMT" "uint32FMT" 1 %s:"uint32FMT" "uint32FMT" "uint32FMT" 1\n",
                fwdMatches[i]->_matchId,
                AF.labelA(), fwdMatches[i]->_iid1, fwdMatches[i]->_acc1->getRangeBegin(), fwdMatches[i]->_acc1->getRangeLength(),
                AF.labelB(), fwdMatches[i]->_iid2, fwdMatches[i]->_acc2->getRangeBegin(), fwdMatches[i]->_acc2->getRangeLength());
      delete fwdMatches[i];
    }

    for (uint32 i=0; i<revMatches.size(); i++) {
      if (!revMatches[i]->isDeleted())
        fprintf(stdout, "M u %s . %s:"uint32FMT" "uint32FMT" "uint32FMT" 1 %s:"uint32FMT" "uint32FMT" "uint32FMT" -1\n",
                revMatches[i]->_matchId,
                AF.labelA(), revMatches[i]->_iid1, revMatches[i]->_acc1->getRangeBegin(), revMatches[i]->_acc1->getRangeLength(),
                AF.labelB(), revMatches[i]->_iid2, revMatches[i]->_acc2->getRangeBegin(), revMatches[i]->_acc2->getRangeLength());
      delete revMatches[i];
    }

    fwdMatches.clear();
    revMatches.clear();
  }

  delete C1;
  delete C2;
}
