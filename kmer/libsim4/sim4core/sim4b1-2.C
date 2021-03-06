#include "sim4.H"

int
Sim4::SIM4_block2(Exon*   &tmp_Lblock,
                  Exon*   &tmp_Rblock,
                  Exon*   &tmp_block,
                  Exon*   &tmp_block1) {
  int cost;
  int rollbflag = 0;

  int diff = (int)(tmp_block1->frEST - tmp_block->toEST - 1);

  //fprintf(stderr, "Called SIM4_block2()\n");

  if (diff <= MAX_GRINIT) {
    cost = greedy(_estSeq + tmp_block->toEST,
                  _genSeq + tmp_block->toGEN, 
                  diff,
                  tmp_block1->frGEN-tmp_block->toGEN-1,
                  tmp_block->toEST,tmp_block->toGEN,
                  &tmp_Lblock, &tmp_Rblock);
#if 0
    printf("greedy returned cost %d (limit:%d)\n", cost, MAX(wordSize,(int)(globalParams->_percentError * diff + 1)));
#endif
  } else {
    cost = MAX(wordSize,(int)(globalParams->_percentError * diff + 1))+1;
  }

  //PRINTEXONS("greedy\n", tmp_Lblock);

  if (cost>MAX(wordSize,(int)(globalParams->_percentError * diff + 1))) {
    if (!tmp_block->flag && !tmp_block1->flag) {
      exon_cores(_genSeq+tmp_block->toGEN-1,
                 _estSeq+tmp_block->toEST-1,
                 tmp_block1->frGEN-tmp_block->toGEN-1,
                 diff,
                 tmp_block->toGEN+1,
                 tmp_block->toEST+1,
                 1,
                 spacedSeedIntMSS,
                 mspThreshold2,
                 TEMP);

      //PRINTEXONS("1\n", exon_list);

      tmp_Lblock = tmp_Rblock = exon_list;
      while ((tmp_Rblock!=NULL) && (tmp_Rblock->next_exon!=NULL)) 
        tmp_Rblock = tmp_Rblock->next_exon;

      if ((!tmp_Lblock && tmp_block1->frGEN-tmp_block->toGEN>50000) ||
          (tmp_Lblock && (tmp_Lblock->frEST-tmp_block->toEST>100) && 
           (tmp_Lblock->frGEN-tmp_block->frGEN>50000)) || 
          (tmp_Lblock && (tmp_block1->frEST-tmp_Rblock->toEST>100) &&
           (tmp_block1->frGEN-tmp_Rblock->frGEN>50000))) {
        /* possible large intron; increase the score weight */
        //freeExonList(tmp_Lblock);  garbage collected

        exon_list = _mspManager.doLinking(globalParams->_relinkWeight,
                                          DEFAULT_DRANGE, 
                                          tmp_block->toGEN + 1,
                                          tmp_block->toEST + 1,
                                          1,
                                          true,
                                          _genSeq, _estSeq); 
                  
        //PRINTEXONS("1a\n", exon_list);

        tmp_Lblock = tmp_Rblock = exon_list;
        while ((tmp_Rblock!=NULL) && (tmp_Rblock->next_exon!=NULL))
          tmp_Rblock = tmp_Rblock->next_exon;
      }    
      _mspManager.clear();

      if (tmp_Lblock)
        rollbflag = 1;
      else
        rollbflag = 0;   /* already 0 */
    } else {
      tmp_Lblock = tmp_Rblock = NULL;
    }
  }

  return(rollbflag);
}
