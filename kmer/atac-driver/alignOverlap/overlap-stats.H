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

#ifndef OVERLAP_STATS_H
#define OVERLAP_STATS_H

#include "overlap.H"

//  Statistics and Histograms
//
//  Index 1 is the assembly, 2 is the mapping.  Stats count the
//  number of bases covered, histograms are of the block sizes.
//
class histogram {
public:
  histogram(uint32 max=65536) {
    histMax = max;
    hist    = new uint32 [histMax];
    sum     = 0;

    for (uint32 i=0; i<histMax; i++)
      hist[i] = 0;
  };
  ~histogram() {
    delete [] hist;
  };

  void  update(uint32 length) {
    sum += length;
    hist[ ((length) > histMax) ? 0 : length ]++;
  };

  void operator+=(uint32 length) {
    update(length);
  };

  uint32  getSum(void) {
    return(sum);
  };

  void writeHistogram(char const *prefix, char const *label) {
    char filename[1024];
    sprintf(filename, "%s.%s", prefix, label);
    errno = 0;
    FILE *out = fopen(filename, "w");
    if (errno)
      fprintf(stderr, "Failed to open '%s': %s\n", filename, strerror(errno)), exit(1);
    for (uint32 i=0; i<histMax; i++)
      fprintf(out, uint32FMT" "uint32FMT"\n", i, hist[i]);
    fclose(out);
  };

private:
  uint32  *hist;
  uint32   histMax;
  uint32   sum;
};

class overlapStats {
public:
  histogram  unmapped;
  histogram  map1unique;
  histogram  map2unique;
  histogram  different;
  histogram  wilddiff;
  histogram  same;
  histogram  inconsistent;

  void writeHistogram(char const *prefix) {
    unmapped.writeHistogram(prefix, "unmapped");
    map1unique.writeHistogram(prefix, "map1unique");
    map2unique.writeHistogram(prefix, "map2unique");
    different.writeHistogram(prefix, "different");
    wilddiff.writeHistogram(prefix, "wilddiff");
    same.writeHistogram(prefix, "same");
    inconsistent.writeHistogram(prefix, "inconsistent");
  };
};


#endif  //  OVERLAP_STATS_H
