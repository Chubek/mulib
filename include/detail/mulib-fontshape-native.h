/* Included only by the opted-in fontshape implementation, after its private type. */
#include "mulib-otlayout.h"
typedef struct mulib_fontshape_native_face_ {
    mulib_typecore_buf_t bytes;
    mulib_sfnt_face_t face;
    mulib_fontcmap_map_t cmap;
    mulib_typecore_slice_t hmtx,hhea,gsub,gpos,kern,head,loca,glyf,gdef,ligatures,pairs;
    uint16_t units,glyphs,longs;
} mulib_fontshape_native_face_;

static int mulib_fontshape_native_scale_(const mulib_fontshape_t *f,int32_t value,int32_t *out) {
    mulib_fontshape_native_face_ *n=(mulib_fontshape_native_face_*)f->native_backend;
    int64_t product=(int64_t)value*f->font_scale;
    /* OpenType/HarfBuzz rounding: nearest integer, ties toward positive infinity. */
    int64_t scaled=product>=0?(product+n->units/2)/n->units:-((-product+(n->units-1)/2)/n->units);
    if(scaled<INT32_MIN||scaled>INT32_MAX)return 0;
    *out=(int32_t)scaled;return 1;
}
static void mulib_fontshape_native_close_(mulib_fontshape_t *f) {
    mulib_fontshape_native_face_ *n=(mulib_fontshape_native_face_*)f->native_backend;
    mulib_typecore_free(&n->bytes);MULIB_FONTSHAPE_FREE(n);f->native_backend=NULL;
}
static mulib_fontshape_t *mulib_fontshape_native_take_(mulib_typecore_buf_t *bytes,unsigned index,int scale) {
    mulib_fontshape_t *f=NULL;mulib_fontshape_native_face_ *n=NULL;
    mulib_typecore_slice_t cmap,maxp,variation;uint16_t location;
    if(scale<=0||!bytes||!bytes->size)return NULL;
    f=(mulib_fontshape_t*)MULIB_FONTSHAPE_CALLOC(1,sizeof(*f));
    n=(mulib_fontshape_native_face_*)MULIB_FONTSHAPE_CALLOC(1,sizeof(*n));
    if(!f||!n)goto fail;
    if(!mulib_sfnt_open_face(&n->face,bytes->data,bytes->size,index)||
       !mulib_sfnt_face_table(&n->face,"head",&n->head)||n->head.size<54||
       !mulib_typecore_read16be(n->head,18,&n->units)||n->units<16||n->units>16384||
       !mulib_sfnt_face_table(&n->face,"maxp",&maxp)||
       !mulib_typecore_read16be(maxp,4,&n->glyphs)||!n->glyphs||
       !mulib_sfnt_face_table(&n->face,"hhea",&n->hhea)||n->hhea.size<36||
       !mulib_typecore_read16be(n->hhea,34,&n->longs)||!n->longs||n->longs>n->glyphs||
       !mulib_sfnt_face_table(&n->face,"hmtx",&n->hmtx)||
       n->hmtx.size<(size_t)n->longs*4+(size_t)(n->glyphs-n->longs)*2||
       !mulib_sfnt_face_table(&n->face,"cmap",&cmap)||
       !mulib_fontcmap_open(&n->cmap,cmap.data,cmap.size)||
       mulib_sfnt_face_table(&n->face,"fvar",&variation))goto fail;
    (void)mulib_sfnt_face_table(&n->face,"GSUB",&n->gsub);
    (void)mulib_sfnt_face_table(&n->face,"GPOS",&n->gpos);
    (void)mulib_sfnt_face_table(&n->face,"kern",&n->kern);
    (void)mulib_sfnt_face_table(&n->face,"GDEF",&n->gdef);
    if(!mulib_otlayout_feature_(n->gsub,0x6c61746eu,0x6c696761u,&n->ligatures)||
       !mulib_otlayout_feature_(n->gpos,0x6c61746eu,0x6b65726eu,&n->pairs)||
       n->ligatures.size>256||n->pairs.size>256)goto fail;
    if(mulib_sfnt_face_table(&n->face,"glyf",&n->glyf)) {
        if(!mulib_sfnt_face_table(&n->face,"loca",&n->loca)||
           !mulib_typecore_read16be(n->head,50,&location)||location>1||
           n->loca.size<((size_t)n->glyphs+1)*(location?4:2))goto fail;
    }
    n->bytes=*bytes;memset(bytes,0,sizeof(*bytes));
    f->native_backend=n;f->font_scale=scale;return f;
fail:
    MULIB_FONTSHAPE_FREE(n);MULIB_FONTSHAPE_FREE(f);return NULL;
}
mulib_fontshape_t *mulib_fontshape_open_memory(const void *data,size_t size,unsigned index,int scale) {
    mulib_typecore_buf_t bytes={0};mulib_fontshape_t *font;
    /* A finite file budget bounds owned memory and directory/table processing. */
    if(!data||!size||size>64u*1024u*1024u||scale<=0||!mulib_typecore_append(&bytes,data,size))return NULL;
    font=mulib_fontshape_native_take_(&bytes,index,scale);mulib_typecore_free(&bytes);return font;
}
mulib_fontshape_t *mulib_fontshape_open_native(const char *path,unsigned index,int scale) {
    FILE *file;long length;mulib_typecore_buf_t bytes={0};mulib_fontshape_t *font=NULL;
    if(!path||scale<=0||!(file=fopen(path,"rb")))return NULL;
    if(fseek(file,0,SEEK_END)||((length=ftell(file))<=0)||
       (unsigned long)length>64u*1024u*1024u||fseek(file,0,SEEK_SET)||
       !mulib_typecore_reserve(&bytes,(size_t)length))goto done;
    if(fread(bytes.data,1,(size_t)length,file)!=(size_t)length)goto done;
    bytes.size=(size_t)length;
    if(fclose(file)){file=NULL;goto done;}
    file=NULL;font=mulib_fontshape_native_take_(&bytes,index,scale);
done:
    if(file)fclose(file);
    mulib_typecore_free(&bytes);return font;
}
static int mulib_fontshape_native_covers_(mulib_fontshape_t *f,uint32_t scalar) {
    mulib_fontshape_native_face_ *n=(mulib_fontshape_native_face_*)f->native_backend;uint32_t glyph;
    if(!mulib_fontcmap_glyph(&n->cmap,scalar,&glyph)||glyph>=n->glyphs)return -1;
    return glyph?1:0;
}
static int mulib_fontshape_native_metrics_(mulib_fontshape_t *f,mulib_fontshape_metrics_t *out) {
    mulib_fontshape_native_face_ *n=(mulib_fontshape_native_face_*)f->native_backend;
    mulib_fontshape_metrics_t result;uint16_t a,b,c;
    if(!out||!mulib_typecore_read16be(n->hhea,4,&a)||!mulib_typecore_read16be(n->hhea,6,&b)||
       !mulib_typecore_read16be(n->hhea,8,&c)||
       !mulib_fontshape_native_scale_(f,a<=INT16_MAX?(int32_t)a:(int32_t)a-65536,&result.ascender)||
       !mulib_fontshape_native_scale_(f,b<=INT16_MAX?(int32_t)b:(int32_t)b-65536,&result.descender)||
       !mulib_fontshape_native_scale_(f,c<=INT16_MAX?(int32_t)c:(int32_t)c-65536,&result.line_gap))return -1;
    *out=result;return 0;
}
static int mulib_fontshape_native_extents_(mulib_fontshape_t *f,uint32_t glyph,mulib_fontshape_extents_t *out) {
    mulib_fontshape_native_face_ *n=(mulib_fontshape_native_face_*)f->native_backend;
    mulib_fontshape_extents_t result={0};uint16_t format;uint32_t first,last;
    if(!out||glyph>=n->glyphs||!n->glyf.data||!mulib_typecore_read16be(n->head,50,&format))return -1;
    if(format) {
        if(!mulib_typecore_read32be(n->loca,(size_t)glyph*4,&first)||
           !mulib_typecore_read32be(n->loca,((size_t)glyph+1)*4,&last))return -1;
    } else {
        uint16_t a,b;
        if(!mulib_typecore_read16be(n->loca,(size_t)glyph*2,&a)||
           !mulib_typecore_read16be(n->loca,((size_t)glyph+1)*2,&b))return -1;
        first=(uint32_t)a*2;last=(uint32_t)b*2;
    }
    if(first>last||last>n->glyf.size)return -1;
    if(first!=last) {
        int32_t values[4];
        if(last-first<10)return -1;
        for(size_t i=0;i<4;i++) {
            uint16_t value;
            if(!mulib_typecore_read16be(n->glyf,(size_t)first+2+i*2,&value))return -1;
            values[i]=value<=INT16_MAX?(int32_t)value:(int32_t)value-65536;
        }
        if(values[0]>values[2]||values[1]>values[3]||
           !mulib_fontshape_native_scale_(f,values[0],&result.x_bearing)||
           !mulib_fontshape_native_scale_(f,values[3],&result.y_bearing)||
           !mulib_fontshape_native_scale_(f,values[2]-values[0],&result.width)||
           !mulib_fontshape_native_scale_(f,values[1]-values[3],&result.height))return -1;
    }
    *out=result;return 0;
}
static int mulib_fontshape_native_carets_(mulib_fontshape_t *f,uint32_t glyph,const char *direction,
                                         int32_t **out,size_t *count) {
    mulib_fontshape_native_face_ *n=(mulib_fontshape_native_face_*)f->native_backend;uint16_t offset;
    if(glyph>=n->glyphs||strcmp(direction,"ltr"))return -1;
    if(n->gdef.size&&(!mulib_typecore_read16be(n->gdef,8,&offset)||offset))return -1;
    *out=NULL;*count=0;return 0;
}
static int mulib_fontshape_native_adjust_(mulib_fontshape_t *f,mulib_fontshape_glyph_t *g,
                                         const mulib_fontgpos_adjustment_t *value) {
    int32_t raw[4]={value->x_advance,value->y_advance,value->x_placement,value->y_placement};
    int32_t old[4]={g->x_advance,g->y_advance,g->x_offset,g->y_offset},result[4];
    for(size_t i=0;i<4;i++) {
        int32_t adjustment;
        if(!mulib_fontshape_native_scale_(f,raw[i],&adjustment))return 0;
        int64_t sum=(int64_t)old[i]+adjustment;
        if(sum<INT32_MIN||sum>INT32_MAX)return 0;
        result[i]=(int32_t)sum;
    }
    g->x_advance=result[0];g->y_advance=result[1];g->x_offset=result[2];g->y_offset=result[3];return 1;
}
static int mulib_fontshape_native_work_(size_t *remaining,size_t cost) {
    if(cost>=*remaining)return 0;
    *remaining-=cost+1;return 1;
}
static int mulib_fontshape_native_unmarked_(mulib_fontshape_native_face_ *n,uint32_t glyph) {
    if(n->gdef.size) {
        uint16_t kind;mulib_fontgdef_view_t view={n->gdef};
        if(!mulib_fontgdef_glyph_class(&view,glyph,&kind)||kind==3)return 0;
    }
    return 1;
}
static int mulib_fontshape_native_shape_(mulib_fontshape_t *f,const char *text,size_t length,
                                        const mulib_fontshape_options_t *o,mulib_fontshape_run_t *out) {
    mulib_fontshape_native_face_ *n=(mulib_fontshape_native_face_*)f->native_backend;
    mulib_fontshape_glyph_t *glyphs=NULL;mulib_typecore_buf_t ids={0};
    size_t count=length,budget=32u*1024u*1024u;int liga=1,kern=1,result=-1;
    if(!out||out->glyphs||out->count||(!text&&length)||length>65536||
       length>SIZE_MAX/sizeof(*glyphs)||length>SIZE_MAX/sizeof(uint32_t))return -1;
    if(o) {
        if((o->direction&&strcmp(o->direction,"ltr"))||(o->script&&strcmp(o->script,"Latn"))||
           (o->language&&strcmp(o->language,"und")&&strcmp(o->language,"en"))||
           o->feature_count>64||(!o->features&&o->feature_count))return -1;
        for(size_t i=0;i<o->feature_count;i++) {
            const char *s=o->features[i];int *flag;
            if(!s)return -1;
            if(!strncmp(s,"liga",4))flag=&liga;else if(!strncmp(s,"kern",4))flag=&kern;else return -1;
            if(!s[4])*flag=1;else if(!strcmp(s+4,"=0"))*flag=0;else if(!strcmp(s+4,"=1"))*flag=1;else return -1;
        }
    }
    for(size_t i=0;i<length;i++)if((unsigned char)text[i]>126||((unsigned char)text[i]<32&&text[i]))return -1;
    if(!length)return 0;
    glyphs=(mulib_fontshape_glyph_t*)MULIB_FONTSHAPE_MALLOC(length*sizeof(*glyphs));
    if(!glyphs||!mulib_typecore_reserve(&ids,length*sizeof(uint32_t)))goto done;
    uint32_t *gids=(uint32_t*)ids.data;
    for(size_t i=0;i<length;i++) {
        if(!mulib_fontcmap_glyph(&n->cmap,(unsigned char)text[i],&gids[i])||gids[i]>=n->glyphs||
           !mulib_fontshape_native_unmarked_(n,gids[i]))goto done;
        glyphs[i]=(mulib_fontshape_glyph_t){gids[i],(uint32_t)i,0,0,0,0};
    }
    if(liga)for(size_t k=0;k<n->ligatures.size/2;k++) {
        uint16_t lookup;mulib_fontgsub_view_t view={n->gsub};
        if(!mulib_typecore_read16be(n->ligatures,k*2,&lookup))goto done;
        for(size_t i=0;i<count;) {
            uint32_t replacement;size_t consumed;
            if(!mulib_fontshape_native_work_(&budget,n->gsub.size)||
               !mulib_fontgsub_substitute(&view,lookup,gids+i,count-i,&replacement,&consumed)||
               replacement>=n->glyphs||!mulib_fontshape_native_unmarked_(n,replacement))goto done;
            gids[i]=replacement;
            if(consumed>1) {
                memmove(gids+i+1,gids+i+consumed,(count-i-consumed)*sizeof(*gids));
                memmove(glyphs+i+1,glyphs+i+consumed,(count-i-consumed)*sizeof(*glyphs));
                count-=consumed-1;
            }
            i++;
        }
    }
    for(size_t i=0;i<count;i++) {
        uint16_t advance;int16_t bearing;
        glyphs[i].glyph=gids[i];
        if(!mulib_fontmetrics_signed(n->hmtx.data,n->hmtx.size,n->longs,(uint16_t)gids[i],&advance,&bearing)||
           !mulib_fontshape_native_scale_(f,advance,&glyphs[i].x_advance))goto done;
    }
    if(kern&&n->pairs.size)for(size_t k=0;k<n->pairs.size/2;k++) {
        uint16_t lookup;mulib_fontgpos_view_t view={n->gpos};
        if(!mulib_typecore_read16be(n->pairs,k*2,&lookup))goto done;
        for(size_t i=0;i+1<count;) {
            mulib_fontgpos_adjustment_t a,b;size_t consumed;
            if(!mulib_fontshape_native_work_(&budget,n->gpos.size)||
               !mulib_fontgpos_pair(&view,lookup,gids[i],gids[i+1],&a,&b,&consumed)||
               !mulib_fontshape_native_adjust_(f,&glyphs[i],&a)||
               !mulib_fontshape_native_adjust_(f,&glyphs[i+1],&b))goto done;
            i+=consumed;
        }
    } else if(kern&&n->kern.size) {
        uint16_t version,tables;size_t at=4;
        if(!mulib_typecore_read16be(n->kern,0,&version)||version||
           !mulib_typecore_read16be(n->kern,2,&tables)||tables>256)goto done;
        for(size_t k=0;k<tables;k++) {
            uint16_t size,coverage;
            if(at>n->kern.size||n->kern.size-at<6||!mulib_typecore_read16be(n->kern,at+2,&size)||
               size<14||size>n->kern.size-at||!mulib_typecore_read16be(n->kern,at+4,&coverage))goto done;
            if(coverage!=1)goto done;
            for(size_t i=0;i+1<count;i++) {
                int16_t value;
                if(!mulib_fontshape_native_work_(&budget,size)||
                   !mulib_fontkern_pair(n->kern.data+at,size,(uint16_t)gids[i],(uint16_t)gids[i+1],&value))goto done;
                mulib_fontgpos_adjustment_t adjustment={0,0,value,0};
                if(!mulib_fontshape_native_adjust_(f,&glyphs[i],&adjustment))goto done;
            }
            at+=size;
        }
    }
    out->glyphs=glyphs;out->count=count;glyphs=NULL;result=0;
done:
    MULIB_FONTSHAPE_FREE(glyphs);mulib_typecore_free(&ids);return result;
}
