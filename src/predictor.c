//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <string.h>
#include "predictor.h"

const char *studentName = "Tao Li";
const char *studentID   = "A53305875";
const char *email       = "t1li@eng.ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

int ghistoryBits; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

int *gpredictTable;
int *lhistoryTable;
int *lpredictTable;
int *selectionTable;
uint32_t BHR;    // Branch history register: recent branch outcomes(length: ghistoryBits)

int perceptronTable_lengthBits;
int8_t **perceptronTable; 
int weight_num;
int theta;


//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//
void
init_predictor()
{
  gpredictTable = (int *)malloc((1<<ghistoryBits) * sizeof(int));
  for (int i = 0; i < (1<<ghistoryBits); i++) gpredictTable[i] = WN;
  lhistoryTable = (int *)malloc((1<<pcIndexBits) * sizeof(int));
  memset(lhistoryTable, 0, (1<<pcIndexBits) * sizeof(int));
  lpredictTable = (int *)malloc((1<<lhistoryBits) * sizeof(int));
  for (int i = 0; i < (1<<lhistoryBits); i++) lpredictTable[i] = WN;
  // 0 Strongly Local 1 Weakly Local 2 Weakly Global 3 Strongly Global
  selectionTable = (int *)malloc((1<<ghistoryBits) * sizeof(int));
  for (int i = 0; i < (1<<ghistoryBits); i++) selectionTable[i] = 2; 
  BHR = 0;
  // custom initialization
  perceptronTable_lengthBits = 8; // There will be 2^8 = 256 entries in perceptronTable
  weight_num = 31; // num of weight factor = history length + 1, we need to track (weight_num-1) bits history
  perceptronTable = (int8_t **) malloc(sizeof(int8_t *) * (1<<perceptronTable_lengthBits));
  for(int i = 0; i < (1<<perceptronTable_lengthBits); i ++)
    perceptronTable[i] = (int8_t *)malloc(sizeof(int8_t) * weight_num);
  for(int i = 0; i < (1<<perceptronTable_lengthBits); i ++)
  {
    for (int j=0;j<weight_num;j++)
    {
      perceptronTable[i][j] = 0;
    }
  }
  theta = (1.93 * (weight_num-1)) + 14;
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t make_gshare_prediction(uint32_t pc){
  uint32_t gindex = (BHR ^ pc)&((1 << ghistoryBits)-1);
  uint8_t pred = gpredictTable[gindex];
  return (pred == WT || pred == ST)?TAKEN:NOTTAKEN;
}

uint8_t make_tournament_prediction(uint32_t pc){
  int select = selectionTable[BHR];
  // 0/1 means local 2/3 means gshare
  if (select<2) {
    uint32_t laddr = pc & ((1<<pcIndexBits) - 1);
    uint32_t lht = lhistoryTable[laddr];
    uint8_t pred = lpredictTable[lht];
    return (pred == WT || pred == ST)?TAKEN:NOTTAKEN;
  } else
  {
    uint8_t pred = gpredictTable[BHR];
    return (pred == WT || pred == ST)?TAKEN:NOTTAKEN;
  }
}

uint8_t make_custom_prediction(uint32_t pc){
  uint32_t ptID = pc & ((1 << perceptronTable_lengthBits) - 1);
  int y = perceptronTable[ptID][0];
  
  for (int i=1;i<weight_num;i++) {
    if ((BHR >> (i-1)) & 1) {
      y += perceptronTable[ptID][i];
    } else {
      y -= perceptronTable[ptID][i];
    }
  }
  if (y>=0) return TAKEN;
  else return NOTTAKEN;

}


uint8_t
make_prediction(uint32_t pc)
{
  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      return make_gshare_prediction(pc);
    case TOURNAMENT:
      return make_tournament_prediction(pc);
    case CUSTOM:
      return make_custom_prediction(pc);
    default:
      break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

uint8_t
update(uint8_t pred, uint8_t outcome) {
  if(outcome == NOTTAKEN){
    if(pred == WN) return SN;
    else if(pred == ST) return WT;
    else if(pred == WT) return WN;
  }
  else{
    if(pred == WT) return ST;
    else if(pred == WN) return WT;
    else if(pred == SN) return WN;
  }
  return pred;
}

int max ( int a, int b ) { return a > b ? a : b; }
int min ( int a, int b ) { return a < b ? a : b; }


void
train_gsahre_predictor(uint32_t pc, uint8_t outcome)
{ 
  uint32_t gindex = (BHR ^ pc)&((1 << ghistoryBits)-1);
  uint8_t pred = gpredictTable[gindex];
  gpredictTable[gindex] = update(pred, outcome);
  BHR = (BHR<<1 | outcome) & ((1<<ghistoryBits) - 1);
}


void
train_tournament_predictor(uint32_t pc, uint8_t outcome)
{ 
  int laddr, lht;
  uint8_t lpredict, p1c;
  laddr = pc & ((1<<pcIndexBits) - 1);
  lht   = lhistoryTable[laddr];
  lhistoryTable[laddr] = (lht << 1 | outcome) & ((1<<lhistoryBits) - 1);
  lpredict = lpredictTable[lht];
  p1c      = ((lpredict<2 && outcome==NOTTAKEN) || (lpredict>1 && outcome==TAKEN))?1:0;
  lpredictTable[lht] = update(lpredict, outcome);


  uint8_t gpredict, p2c;
  gpredict   = gpredictTable[BHR];
  p2c        = ((gpredict<2 && outcome==NOTTAKEN) || (gpredict>1 && outcome==TAKEN))?1:0;
  gpredictTable[BHR] = update(gpredict, outcome);


  if (p1c - p2c == -1) { 
    selectionTable[BHR] = min(3,selectionTable[BHR] + 1);
  } else if(p1c - p2c == 1) {
    selectionTable[BHR] = max(0,selectionTable[BHR] - 1);
  }
  BHR = (BHR<<1 | outcome) & ((1<<ghistoryBits) - 1);
}

void
train_custom_predictor(uint32_t pc, uint8_t outcome)
{ 
  
  int ptID = pc & ((1<<perceptronTable_lengthBits) - 1);
  int y = perceptronTable[ptID][0];
  for (int i=1;i<weight_num;i++) {
    if ((BHR >> (i-1)) & 1) {
      y += perceptronTable[ptID][i];
    } else {
      y -= perceptronTable[ptID][i];
    }
  }
  if (abs(y) <= theta || ((y>=0)!=outcome)) {
    for (int i=0;i<weight_num;i++) {
      if (i==0) {
        if (outcome == TAKEN)
        {
          if (perceptronTable[ptID][i]<127)
          {
            perceptronTable[ptID][i] += 1;
          }
        } else
        {
          if (perceptronTable[ptID][i]>-128)
          {
            perceptronTable[ptID][i] -= 1;
          }
        }
      } else if (((BHR>>(i-1)) & 1)==outcome)
      {
        if (perceptronTable[ptID][i]<127)
        {
          perceptronTable[ptID][i] += 1;
        }
      } else
      {
        if (perceptronTable[ptID][i]>-128)
        {
          perceptronTable[ptID][i] -= 1;
        }
      }
    }
  }
  BHR = (BHR<<1 | outcome) & ((1<<weight_num) - 1);
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void
train_predictor(uint32_t pc, uint8_t outcome)
{    
  switch (bpType) {
    case GSHARE:
      train_gsahre_predictor(pc, outcome);
      break;
    case TOURNAMENT:
      train_tournament_predictor(pc, outcome);
      break;
    case CUSTOM:
      train_custom_predictor(pc, outcome);
      break;
    default:
      break;
  }
}
