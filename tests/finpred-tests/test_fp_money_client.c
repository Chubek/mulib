/* Batch bridge for the independent Python integer oracle; declaration-only. */
#include "mulib-corporate-actions.h"
#include "mulib-fxrate.h"
#include "mulib-orderintent.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int main(void){
    char operation;
    int64_t x,n,d;
    int scanned;
    while((scanned=scanf(" %c %" SCNd64 " %" SCNd64 " %" SCNd64,&operation,&x,&n,&d))==4){
        int ok=0;
        const int64_t sentinel=-12345;
        int64_t value=sentinel;
        mulib_money_t source={0},out={0},saved;
        assert(mulib_money_make(&source,x,"EUR",2));
        assert(mulib_money_make(&out,value,"USD",4));
        memcpy(&saved,&out,sizeof(out));
        switch(operation){
        case 's':
            ok=mulib_corporate_actions_adjust_shares(x,(mulib_corporate_actions_split_t){n,d},&value);
            assert(ok||value==sentinel);
            break;
        case 'p':
            ok=mulib_corporate_actions_adjust_price(source,(mulib_corporate_actions_split_t){n,d},&out);
            break;
        case 'd':
            ok=mulib_corporate_actions_dividend(source,n,&out);
            break;
        case 'f':{
            mulib_fxrate_t rate={{'E','U','R','\0'},{'J','P','Y','\0'},2,0,n,d,100};
            ok=mulib_fxrate_convert_asof(source,&rate,120,120,20,&out);
            break;
        }
        case 'o':{
            mulib_orderintent_t intent={1,2,MULIB_ORDERINTENT_BUY,n,source,100,110};
            mulib_orderintent_policy_t policy={INT64_MAX,source,1,1,0};
            policy.max_notional.minor=d;
            ok=mulib_orderintent_validate(&intent,&policy,100,&out);
            break;
        }
        default:
            return 2;
        }
        if(operation!='s'){
            if(ok){
                assert(mulib_money_valid(out));
                assert(!memcmp(out.currency,operation=='f'?"JPY":"EUR",4));
                assert(out.decimals==(operation=='f'?0:2));
            }else assert(!memcmp(&out,&saved,sizeof(out)));
            value=out.minor;
        }
        printf("%d %" PRId64 "\n",ok,value);
    }
    return scanned==EOF?0:1;
}
