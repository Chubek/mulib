static int mulib_docserialize_status_(mulib_docir_report_t *r,unsigned code,size_t node) {
    if(r)*r=(mulib_docir_report_t){code,node};
    return code==mulib_docir_OK;
}
static int mulib_docserialize_u64_(mulib_typecore_buf_t *out,size_t value) {
    uint64_t v=value;return mulib_typecore_u32be(out,(uint32_t)(v>>32))&&mulib_typecore_u32be(out,(uint32_t)v);
}
static int mulib_docserialize_string_(mulib_typecore_buf_t *out,const mulib_docir_snapshot_t *s,size_t at) {
    if(at==SIZE_MAX)return mulib_typecore_u32be(out,UINT32_MAX);
    const char *text=mulib_docir_snapshot_string(s,at);if(!text)return 0;
    size_t length=strlen(text);
    return length<UINT32_MAX&&mulib_typecore_u32be(out,(uint32_t)length)&&mulib_typecore_append(out,text,length);
}
int mulib_docserialize_tree(const mulib_docir_node_t *root,const mulib_docir_limits_t *limits,
                          mulib_typecore_buf_t *out,mulib_docir_report_t *report) {
    mulib_docir_snapshot_t snapshot={0};mulib_typecore_buf_t bytes={0};int ok=0;
    if(!out)return mulib_docserialize_status_(report,mulib_docir_E_ARGUMENT,0);
    if(!mulib_docir_snapshot_make(root,limits,&snapshot,report))return 0;
    if(!mulib_docir_snapshot_validate(&snapshot,0,report))goto done;
    if(!mulib_typecore_append(&bytes,"MuDocIR\1",8)||!mulib_typecore_u32be(&bytes,(uint32_t)snapshot.count))goto memory;
    for(size_t i=0;i<snapshot.count;i++) {
        const mulib_docir_record_t *r=snapshot.records+i;
        if(!mulib_typecore_u32be(&bytes,r->kind)||!mulib_typecore_u32be(&bytes,r->parent==SIZE_MAX?UINT32_MAX:(uint32_t)r->parent)||
           !mulib_typecore_u32be(&bytes,r->level)||!mulib_typecore_u32be(&bytes,r->style.flags)||
           !mulib_typecore_u32be(&bytes,(uint32_t)r->style.font_size_sp)||!mulib_typecore_u32be(&bytes,(uint32_t)r->style.indent_sp)||
           !mulib_docserialize_u64_(&bytes,r->source_begin)||!mulib_docserialize_u64_(&bytes,r->source_end)||
           !mulib_docserialize_u64_(&bytes,r->source_line)||!mulib_docserialize_u64_(&bytes,r->source_column)||
           !mulib_docserialize_string_(&bytes,&snapshot,r->text)||!mulib_docserialize_string_(&bytes,&snapshot,r->id)||
           !mulib_docserialize_string_(&bytes,&snapshot,r->target)||!mulib_docserialize_string_(&bytes,&snapshot,r->source))goto memory;
    }
    if(!mulib_typecore_append(out,bytes.data,bytes.size))goto memory;
    ok=mulib_docserialize_status_(report,mulib_docir_OK,SIZE_MAX);goto done;
memory:mulib_docserialize_status_(report,mulib_docir_E_MEMORY,0);
done:mulib_typecore_free(&bytes);mulib_docir_snapshot_destroy(&snapshot);return ok;
}
static int mulib_docserialize_get32_(mulib_typecore_slice_t input,size_t *at,uint32_t *value) {
    if(!mulib_typecore_read32be(input,*at,value))return 0;
    *at+=4;return 1;
}
static int mulib_docserialize_get64_(mulib_typecore_slice_t input,size_t *at,size_t *value) {
    uint32_t high,low;
    if(!mulib_docserialize_get32_(input,at,&high)||!mulib_docserialize_get32_(input,at,&low))return 0;
    uint64_t v=((uint64_t)high<<32)|low;if(v>SIZE_MAX)return 0;*value=(size_t)v;return 1;
}
static int mulib_docserialize_getstr_(mulib_typecore_slice_t input,size_t *at,mulib_typecore_buf_t *strings,size_t limit,size_t *offset) {
    uint32_t length;*offset=SIZE_MAX;
    if(!mulib_docserialize_get32_(input,at,&length))return 0;
    if(length==UINT32_MAX)return 1;
    if(*at>input.size||length>input.size-*at||strings->size>=limit||length>=limit-strings->size||
       memchr(input.data+*at,0,length))return 0;
    *offset=strings->size;
    if(!mulib_typecore_append(strings,input.data+*at,length)||!mulib_typecore_byte(strings,0))return 0;
    *at+=length;return 1;
}
int mulib_docserialize_read(const void *data,size_t length,const mulib_docir_limits_t *limits,
                          mulib_docir_node_t **out,mulib_docir_report_t *report) {
    mulib_typecore_slice_t input={(const unsigned char*)data,length};mulib_typecore_buf_t storage={0},strings={0};
    mulib_docir_limits_t budget=limits?*limits:(mulib_docir_limits_t){0,0,0};
    size_t at=8,index=0;uint32_t count;int ok=0;
    if(!budget.max_nodes)budget.max_nodes=100000;
    if(!budget.max_depth)budget.max_depth=256;
    if(!budget.max_text_bytes)budget.max_text_bytes=16u*1024u*1024u;
    if(!out||*out||!data||length<12||length>160u*1024u*1024u||memcmp(data,"MuDocIR\1",8)||
       budget.max_nodes>1000000||budget.max_depth>4096||budget.max_text_bytes>64u*1024u*1024u||
       !mulib_docserialize_get32_(input,&at,&count)||!count||count>budget.max_nodes||
       count>(length-12)/72)
        return mulib_docserialize_status_(report,mulib_docir_E_ARGUMENT,0);
    size_t record_count=count;
    if(record_count>SIZE_MAX/sizeof(mulib_docir_record_t))return mulib_docserialize_status_(report,mulib_docir_E_LIMIT,0);
    if(!mulib_typecore_reserve(&storage,record_count*sizeof(mulib_docir_record_t)))
        return mulib_docserialize_status_(report,mulib_docir_E_MEMORY,0);
    mulib_docir_record_t *records=(mulib_docir_record_t*)storage.data;
    memset(records,0,(size_t)count*sizeof(*records));
    for(index=0;index<count;index++) {
        mulib_docir_record_t *r=records+index;uint32_t kind,parent,level,flags,font,indent;
        if(!mulib_docserialize_get32_(input,&at,&kind)||!mulib_docserialize_get32_(input,&at,&parent)||
           !mulib_docserialize_get32_(input,&at,&level)||!mulib_docserialize_get32_(input,&at,&flags)||
           !mulib_docserialize_get32_(input,&at,&font)||!mulib_docserialize_get32_(input,&at,&indent)||
           !mulib_docserialize_get64_(input,&at,&r->source_begin)||!mulib_docserialize_get64_(input,&at,&r->source_end)||
           !mulib_docserialize_get64_(input,&at,&r->source_line)||!mulib_docserialize_get64_(input,&at,&r->source_column)||
           !mulib_docserialize_getstr_(input,&at,&strings,budget.max_text_bytes,&r->text)||
           !mulib_docserialize_getstr_(input,&at,&strings,budget.max_text_bytes,&r->id)||
           !mulib_docserialize_getstr_(input,&at,&strings,budget.max_text_bytes,&r->target)||
           !mulib_docserialize_getstr_(input,&at,&strings,budget.max_text_bytes,&r->source))goto invalid;
        r->kind=kind;r->level=level;r->style.flags=flags;
        r->style.font_size_sp=(int32_t)(font<=INT32_MAX?(int64_t)font:(int64_t)font-INT64_C(4294967296));
        r->style.indent_sp=(int32_t)(indent<=INT32_MAX?(int64_t)indent:(int64_t)indent-INT64_C(4294967296));
        if(parent==UINT32_MAX){if(index)goto invalid;r->parent=SIZE_MAX;}
        else {
            if(parent>=index)goto invalid;
            r->parent=parent;r->depth=records[parent].depth+1;records[parent].child_count++;
            if(r->depth>budget.max_depth)goto invalid;
        }
    }
    if(at!=length)goto invalid;
    mulib_docir_snapshot_t view={records,count,strings};
    if(!mulib_docir_snapshot_validate(&view,0,report))goto done;
    mulib_docir_node_t *root=mulib_docir_build(&view,0,report);if(!root)goto done;
    *out=root;ok=mulib_docserialize_status_(report,mulib_docir_OK,SIZE_MAX);goto done;
invalid:mulib_docserialize_status_(report,mulib_docir_E_STRUCTURE,index);
done:mulib_typecore_free(&strings);mulib_typecore_free(&storage);return ok;
}
