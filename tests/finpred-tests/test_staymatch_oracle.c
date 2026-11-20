/* Independent miniature exhaustive occupancy oracle for StayMatch.
 * Intentionally does not call or reproduce the kernel's dynamic programming. */
#define MULIB_IMPL_STAYMATCH
#include "mulib-staymatch.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mulib_money_t money(int64_t x){mulib_money_t m;assert(mulib_money_make(&m,x,"EUR",2));return m;}
static int assign_rooms(const mulib_staymatch_room_t **selected,size_t n,size_t idx,
                        int adults,int children,int require_adult){
    int a,c;
    const mulib_staymatch_room_t *r;
    if(idx==n)return adults==0&&children==0;
    r=selected[idx];
    for(a=require_adult?1:0;a<=adults&&a<=r->adult_capacity;++a)
        for(c=0;c<=children&&c<=r->child_capacity;++c){
            if(a+c==0||a+c>r->total_capacity)continue;
            if(assign_rooms(selected,n,idx+1,adults-a,children-c,require_adult))return 1;
        }
    return 0;
}
static void check_bundle(const mulib_staymatch_bundle_t *b,int64_t cost,
                         unsigned rooms,const uint8_t *selection,size_t n){
    size_t i;
    assert(b->total_price.minor==cost && b->total_price.decimals==2 &&
           !memcmp(b->total_price.currency,"EUR",4) && b->room_count==rooms);
    assert(!memcmp(b->selected,selection,n));
    for(i=n;i<MULIB_STAYMATCH_MAX_TYPES;++i)assert(!b->selected[i]);
}
static unsigned rnd(void){
    static uint32_t state=UINT32_C(0x3ba5f101);
    state=state*UINT32_C(1664525)+UINT32_C(1013904223);
    return state>>16;
}
/* Individually assign each guest to a labeled unit. This does not use
 * lower-bound flow or the legacy capacity DP. */
static int place(const mulib_staymatch_request_t *q,const uint8_t *ages,
    const mulib_staymatch_dated_room_t **units,size_t n,unsigned guest,
    uint8_t *ad,uint8_t *ch){
    size_t i;
    if(guest==(unsigned)q->adults+q->children){
        for(i=0;i<n;++i)
            if(!(ad[i]+ch[i]) || ad[i]<units[i]->min_adults ||
               (q->require_adult_per_room&&!ad[i]))return 0;
        return 1;
    }
    for(i=0;i<n;++i){
        const mulib_staymatch_dated_room_t *d=units[i];
        uint8_t *used=&ad[i];
        if(ad[i]+ch[i]>=d->room.total_capacity)continue;
        if(guest<q->adults){
            if(ad[i]>=d->room.adult_capacity)continue;
        }else{
            uint8_t age=ages[guest-q->adults];
            if(ch[i]>=d->room.child_capacity||age<d->min_child_age||age>d->max_child_age)continue;
            used=&ch[i];
        }
        ++*used;
        if(place(q,ages,units,n,guest+1,ad,ch))return 1;
        --*used;
    }
    return 0;
}
typedef struct oracle_candidate {
    int64_t price;
    unsigned units;
    uint8_t k[4];
} oracle_candidate_t;
static int dated_order(const void *a,const void *b){
    const oracle_candidate_t *x=(const oracle_candidate_t *)a,*y=(const oracle_candidate_t *)b;
    unsigned i;
    if(x->price!=y->price)return x->price<y->price?-1:1;
    if(x->units!=y->units)return x->units<y->units?-1:1;
    for(i=0;i<4;++i)if(x->k[i]!=y->k[i])return x->k[i]>y->k[i]?-1:1;
    return 0;
}
static void oracle_dated(const mulib_staymatch_request_t *q,
    const mulib_staymatch_dated_room_t *d,const uint8_t *ages){
    oracle_candidate_t winners[81];
    mulib_staymatch_dated_room_t reverse[4];
    mulib_staymatch_bundle_t out[32];
    unsigned key,i,j,cap;
    size_t n=0,got;
    for(key=1;key<81;++key){
        oracle_candidate_t c={0};
        const mulib_staymatch_dated_room_t *units[8];
        uint8_t ad[8]={0},ch[8]={0};
        unsigned digits=key,shared=0;
        for(i=0;i<4;++i){
            const mulib_staymatch_dated_room_t *r=&d[i];
            unsigned k=digits%3;
            digits/=3;
            if(k==0)continue;
            if(k>r->room.available_units || c.units+k>q->max_rooms ||
               q->check_in_day<r->room.available_from_day ||
               q->check_out_day>r->room.available_until_day ||
               q->check_in_day!=r->quoted_check_in_day ||
               q->check_out_day!=r->quoted_check_out_day ||
               r->book_from>5 || r->book_until<=5)break;
            if(r->inventory_group){
                shared+=k;
                if(shared>r->inventory_limit)break;
            }
            c.k[i]=(uint8_t)k;
            c.price+=(int64_t)k*r->room.full_stay_price.minor;
            for(j=0;j<k;++j)units[c.units++]=r;
        }
        if(i==4&&place(q,ages,units,c.units,0,ad,ch))winners[n++]=c;
    }
    qsort(winners,n,sizeof(*winners),dated_order);
    for(i=0;i<4;++i)reverse[i]=d[3-i];
    for(cap=1;cap<=32;cap=cap==1?3:cap==3?32:33){
        got=99;
        assert(mulib_staymatch_solve_dated(q,5,ages,q->children,d,4,out,cap,&got));
        assert(got==(n<cap?n:cap));
        for(i=0;i<got;++i)
            check_bundle(&out[i],winners[i].price,winners[i].units,winners[i].k,4);
        assert(mulib_staymatch_solve_dated(q,5,ages,q->children,reverse,4,out,cap,&got));
        assert(got==(n<cap?n:cap));
        for(i=0;i<got;++i){
            uint8_t k[4];
            for(j=0;j<4;++j)k[j]=winners[i].k[3-j];
            check_bundle(&out[i],winners[i].price,winners[i].units,k,4);
        }
    }
}
static void dated_cases(void){
    mulib_staymatch_request_t q={1,100,105,1,2,4,0,"EUR",2,20000};
    const uint8_t ages[]={2,14,6};
    mulib_staymatch_dated_room_t d[4];
    unsigned trial,i;
    for(trial=0;trial<2400;++trial){
        q.adults=(uint8_t)(rnd()%3+1);q.children=(uint8_t)(rnd()%4);
        q.require_adult_per_room=(uint8_t)(rnd()%2);q.max_rooms=(uint8_t)(rnd()%4+1);
        for(i=0;i<4;++i){
            unsigned cap=rnd()%4+1,ac=rnd()%(cap+1),cc=rnd()%(cap+1);
            d[i]=(mulib_staymatch_dated_room_t){
                {1,i+1,90,110,(uint8_t)ac,(uint8_t)cc,(uint8_t)cap,(uint8_t)(rnd()%3),money(rnd()%3)},
                100,105,5,8,(uint8_t)(rnd()%(ac+1)),0,17,0,0};
            if(rnd()%4==0)d[i].min_child_age=7;
            if(rnd()%4==0)d[i].max_child_age=10;
            if(rnd()%8==0)d[i].quoted_check_in_day=101;
            if(rnd()%8==0)d[i].quoted_check_out_day=104;
            if(rnd()%8==0)d[i].book_from=6;
            if(rnd()%8==0)d[i].room.available_until_day=104;
            if(rnd()%2){d[i].inventory_group=1;d[i].inventory_limit=2;}
        }
        oracle_dated(&q,d,ages);
    }
}
static void boundary_cases(void){
    mulib_staymatch_request_t q={1,INT64_MIN,INT64_MAX,1,1,8,0,"EUR",2,1000};
    mulib_staymatch_dated_room_t d[MULIB_STAYMATCH_MAX_TYPES];
    mulib_staymatch_bundle_t out[32];
    unsigned char saved[sizeof(out)];
    size_t i,n=44;
    uint8_t ages[16]={17};
    for(i=0;i<MULIB_STAYMATCH_MAX_TYPES;++i){
        d[i]=(mulib_staymatch_dated_room_t){
            {1,(uint32_t)i+1,INT64_MIN,INT64_MAX,1,1,2,8,money((int64_t)i)},
            INT64_MIN,INT64_MAX,5,6,0,0,17,0,0};
    }
    /* Many products/candidates, but the zero-total one-room bundle proves a
     * tight bound. A tiny budget cannot publish a partial result even if
     * it can afford the first feasible bundle. */
    assert(mulib_staymatch_solve_dated(&q,5,ages,1,d,24,out,1,&n));
    assert(n==1&&out[0].total_price.minor==0&&out[0].room_count==1);
    q.max_search_nodes=25;
    memset(out,0x5a,sizeof(out));memcpy(saved,out,sizeof(out));n=44;
    assert(!mulib_staymatch_solve_dated(&q,5,ages,1,d,24,out,1,&n));
    assert(n==44&&!memcmp(out,saved,sizeof(out)));
    /* Even though aggregate stock can fit nine adults, eight rooms cannot.
     * A root-state bound can certify absence under a one-state budget. */
    q.adults=9;q.max_search_nodes=1;
    assert(mulib_staymatch_solve_dated(&q,5,ages,1,d,24,out,1,&n)&&n==0);
    assert(!memcmp(out,saved,sizeof(out)));
    q.adults=1;
    q.max_search_nodes=1000;
    d[0].quoted_check_out_day=1;
    assert(mulib_staymatch_solve_dated(&q,5,ages,1,d,1,out,1,&n)&&n==0);
    assert(!memcmp(out,saved,sizeof(out)));
    d[0].quoted_check_out_day=INT64_MAX;
    assert(mulib_staymatch_solve_dated(&q,6,ages,1,d,1,out,1,&n)&&n==0);
    q.adults=16;q.children=16;
    d[0].room.adult_capacity=2;d[0].room.child_capacity=2;d[0].room.total_capacity=4;
    d[0].min_adults=2;d[0].room.full_stay_price=money(INT64_MAX/8);
    assert(mulib_staymatch_solve_dated(&q,5,ages,16,d,1,out,32,&n));
    assert(n==1&&out[0].room_count==8&&out[0].total_price.minor==INT64_MAX-7);
    d[0].room.full_stay_price=money(INT64_MAX/8+1);
    assert(mulib_staymatch_solve_dated(&q,5,ages,16,d,1,out,32,&n)&&n==0);
    d[0].room.full_stay_price=money(1);
    d[0].min_child_age=1;
    assert(mulib_staymatch_solve_dated(&q,5,ages,16,d,1,out,32,&n)&&n==0);
    memcpy(out,saved,sizeof(out));n=44;
    d[0].quoted_check_out_day=INT64_MIN;
    assert(!mulib_staymatch_solve_dated(&q,5,ages,16,d,1,out,32,&n));
    d[0].quoted_check_out_day=INT64_MAX;
    assert(!mulib_staymatch_solve_dated(&q,5,NULL,16,d,1,out,32,&n));
    assert(!mulib_staymatch_solve_dated(&q,5,ages,15,d,1,out,32,&n));
    ages[5]=18;
    assert(!mulib_staymatch_solve_dated(&q,5,ages,16,d,1,out,32,&n));
    ages[5]=1;
    d[0].book_from=-1;
    assert(!mulib_staymatch_solve_dated(&q,5,ages,16,d,1,out,32,&n));
    d[0].book_from=5;
    d[0].inventory_group=d[1].inventory_group=1;
    d[0].inventory_limit=1;d[1].inventory_limit=2;
    assert(!mulib_staymatch_solve_dated(&q,5,ages,16,d,2,out,32,&n));
    d[1].inventory_limit=1;
    d[1].room.property_id=2;
    assert(!mulib_staymatch_solve_dated(&q,5,ages,16,d,2,out,32,&n));
    assert(n==44&&!memcmp(out,saved,sizeof(out)));
}
static void inventory_cases(void){
    const mulib_staymatch_request_t q={1,100,105,2,2,2,1,"EUR",2,200};
    const uint8_t ages[]={2,13};
    mulib_staymatch_dated_room_t d[]={
        {{1,2,100,105,1,1,2,2,{0}},100,105,5,6,1,0,9,1,2},
        {{1,1,100,105,1,1,2,2,{0}},100,105,5,6,1,10,17,1,2}
    };
    mulib_staymatch_bundle_t out[2];
    size_t n;
    d[0].room.full_stay_price=money(20);d[1].room.full_stay_price=money(30);
    assert(mulib_staymatch_solve_dated(&q,5,ages,2,d,2,out,2,&n)&&n==1);
    assert(out[0].total_price.minor==50&&out[0].selected[0]==1&&out[0].selected[1]==1);
    d[0].inventory_limit=d[1].inventory_limit=1;
    assert(mulib_staymatch_solve_dated(&q,5,ages,2,d,2,out,2,&n)&&n==0);
    d[0].inventory_limit=d[1].inventory_limit=2;
    d[0].max_child_age=1;
    assert(mulib_staymatch_solve_dated(&q,5,ages,2,d,2,out,2,&n)&&n==0);
}
int main(void){
    int ac,cc,a1,c1,t1,a2,c2,t2,require;
    unsigned tested=0;
    for(ac=1;ac<=4;++ac)for(cc=0;cc<=3;++cc)
    for(a1=1;a1<=3;++a1)for(c1=0;c1<=2;++c1)for(t1=1;t1<=4;++t1)
    for(a2=1;a2<=3;++a2)for(c2=0;c2<=2;++c2)for(t2=1;t2<=4;++t2)
    for(require=0;require<=1;++require){
        mulib_staymatch_request_t q={0};
        mulib_staymatch_room_t room[2]={0};
        mulib_staymatch_bundle_t best[5]={0};
        const mulib_staymatch_room_t *selected[3];
        uint8_t expected[5][2];
        int prices[5],k0,k1;
        size_t out_count=0,feasible=0,i;
        q.property_id=1;q.check_in_day=100;q.check_out_day=105;
        q.adults=(uint8_t)ac;q.children=(uint8_t)cc;
        q.max_rooms=3;q.require_adult_per_room=(uint8_t)require;
        q.max_search_nodes=200;memcpy(q.currency,"EUR",4);q.decimals=2;
        room[0]=(mulib_staymatch_room_t){1,1,90,110,(uint8_t)a1,(uint8_t)c1,
               (uint8_t)t1,1,{0}};
        room[1]=(mulib_staymatch_room_t){1,2,90,110,(uint8_t)a2,(uint8_t)c2,
               (uint8_t)t2,2,{0}};
        if(a1>t1||c1>t1||a2>t2||c2>t2)continue;
        room[0].full_stay_price=money(100);
        room[1].full_stay_price=money(130);
        for(k0=0;k0<=1;++k0)for(k1=0;k1<=2;++k1){
            int total=k0+k1,p;
            if(total==0||total>3)continue;
            p=0;
            if(k0)selected[p++]=&room[0];
            while(p<total)selected[p++]=&room[1];
            if(assign_rooms(selected,(size_t)total,0,ac,cc,require)){
                int value=100*k0+130*k1;
                i=feasible++;
                while(i && value<prices[i-1]){
                    prices[i]=prices[i-1];memcpy(expected[i],expected[i-1],2);--i;
                }
                prices[i]=value;expected[i][0]=(uint8_t)k0;expected[i][1]=(uint8_t)k1;
            }
        }
        assert(mulib_staymatch_solve(&q,room,2,best,5,&out_count)&&out_count==feasible);
        for(i=0;i<feasible;++i)
            check_bundle(&best[i],prices[i],expected[i][0]+expected[i][1],expected[i],2);
        assert(mulib_staymatch_solve(&q,room,2,best,1,&out_count)&&out_count==(feasible!=0));
        if(feasible)check_bundle(&best[0],prices[0],expected[0][0]+expected[0][1],expected[0],2);
        ++tested;
    }
    assert(tested>10000);
    dated_cases();
    boundary_cases();
    inventory_cases();
    printf("StayMatch differential scenarios passed: %u legacy + 2400 dated\n",tested);
    return 0;
}
