typedef struct mulib_docir_id_entry_ { const char *id;size_t node;unsigned kind; } mulib_docir_id_entry_;
typedef struct mulib_docir_counts_ { size_t children;unsigned captions; } mulib_docir_counts_;
static int mulib_docir_id_compare_(const void *a,const void *b) {
    return strcmp(((const mulib_docir_id_entry_*)a)->id,((const mulib_docir_id_entry_*)b)->id);
}
static int mulib_docir_inline_(unsigned kind) {
    return kind==mulib_docir_TEXT||kind==mulib_docir_LINK||kind==mulib_docir_MATH||
           kind==mulib_docir_REFERENCE||kind==mulib_docir_CITE||kind==mulib_docir_FOOTNOTE||
           kind==mulib_docir_LINEBREAK||kind==mulib_docir_CODE;
}
static int mulib_docir_math_(unsigned kind) {
    return kind>=mulib_docir_MATH_ROW&&kind<=mulib_docir_MATH_RADICAL;
}
static int mulib_docir_child_(unsigned parent,unsigned child) {
    if(child==mulib_docir_ROOT)return 0;
    if(parent==mulib_docir_MATH||mulib_docir_math_(parent))return mulib_docir_math_(child);
    if(mulib_docir_math_(child))return 0;
    if(parent==mulib_docir_LIST)return child==mulib_docir_ITEM;
    if(parent==mulib_docir_BIBLIOGRAPHY)return child==mulib_docir_BIBITEM;
    if(parent==mulib_docir_PARAGRAPH||parent==mulib_docir_HEADING||parent==mulib_docir_CAPTION||
       parent==mulib_docir_LINK||parent==mulib_docir_FOOTNOTE)return mulib_docir_inline_(child)&&
           !(parent==mulib_docir_LINK&&(child==mulib_docir_LINK||child==mulib_docir_REFERENCE||child==mulib_docir_CITE))&&
           !((parent==mulib_docir_LINK||parent==mulib_docir_HEADING||parent==mulib_docir_CAPTION||parent==mulib_docir_FOOTNOTE)&&child==mulib_docir_FOOTNOTE);
    if(child==mulib_docir_ITEM||child==mulib_docir_BIBITEM)return 0;
    if(child==mulib_docir_CAPTION)return parent==mulib_docir_FIGURE;
    if(parent==mulib_docir_FIGURE&&child==mulib_docir_FIGURE)return 0;
    return parent==mulib_docir_ROOT||parent==mulib_docir_SECTION||parent==mulib_docir_ITEM||
           parent==mulib_docir_FIGURE||parent==mulib_docir_BIBITEM;
}
static const char *mulib_docir_checked_string_(const mulib_docir_snapshot_t *s,size_t at,size_t *budget) {
    if(at==SIZE_MAX)return NULL;
    if(!s->strings.data||at>=s->strings.size||!*budget)return NULL;
    size_t available=s->strings.size-at;if(available>*budget)available=*budget;
    const unsigned char *end=(const unsigned char*)memchr(s->strings.data+at,0,available);
    if(!end)return NULL;
    size_t length=(size_t)(end-(s->strings.data+at));
    if(!mulib_docir_text_valid_((const char*)s->strings.data+at,length))return NULL;
    *budget-=length+1;return (const char*)s->strings.data+at;
}
int mulib_docir_snapshot_validate(const mulib_docir_snapshot_t *s,unsigned flags,mulib_docir_report_t *report) {
    mulib_docir_counts_ *children=NULL;mulib_docir_id_entry_ *ids=NULL;size_t id_count=0,index=0,ancestors[4097];
    unsigned error=mulib_docir_E_STRUCTURE;
    if(!s||!s->records||!s->count||s->count>1000000||s->strings.size>64u*1024u*1024u||
       s->strings.size>s->strings.capacity||(!s->strings.data&&s->strings.size)||flags&~3u||
       s->count>SIZE_MAX/sizeof(*children)||s->count>SIZE_MAX/sizeof(*ids))
        return mulib_docir_report_(report,mulib_docir_E_ARGUMENT,0);
    children=(mulib_docir_counts_*)MULIB_DOCIR_CALLOC(s->count,sizeof(*children));
    ids=(mulib_docir_id_entry_*)MULIB_DOCIR_CALLOC(s->count,sizeof(*ids));
    if(!children||!ids){error=mulib_docir_E_MEMORY;goto done;}
    size_t text_budget=s->strings.size;
    for(index=0;index<s->count;index++) {
        const mulib_docir_record_t *r=s->records+index;
        if(r->kind>mulib_docir_CODE||r->depth>4096||r->level<1||r->level>6||!mulib_docir_style_valid_(r->style))goto done;
        if(r->kind!=mulib_docir_HEADING&&r->kind!=mulib_docir_SECTION&&r->level!=1)goto done;
        if(!index) {
            if(r->parent!=SIZE_MAX||r->depth||((flags&mulib_docir_CHECK_DOCUMENT)&&r->kind!=mulib_docir_ROOT))goto done;
        } else {
            if(r->parent>=index||!r->depth||r->depth>s->records[index-1].depth+1||
               r->depth!=s->records[r->parent].depth+1||ancestors[r->depth-1]!=r->parent||
               !mulib_docir_child_(s->records[r->parent].kind,r->kind))goto done;
            children[r->parent].children++;
            if(r->kind==mulib_docir_CAPTION&&
               (children[r->parent].captions++||
                (children[r->parent].children!=1&&children[r->parent].children!=s->records[r->parent].child_count)))goto done;
        }
        ancestors[r->depth]=index;
        size_t offsets[]={r->text,r->id,r->target,r->source};const char *values[4]={NULL,NULL,NULL,NULL};
        for(size_t j=0;j<4;j++) {
            values[j]=mulib_docir_checked_string_(s,offsets[j],&text_budget);
            if(offsets[j]!=SIZE_MAX&&!values[j]){error=mulib_docir_E_TEXT;goto done;}
        }
        if(values[1]) {
            if(!mulib_docir_id_(values[1])){error=mulib_docir_E_TEXT;goto done;}
            ids[id_count++]=(mulib_docir_id_entry_){values[1],index,r->kind};
        }
        if(mulib_docir_math_(r->kind)&&(values[1]||r->style.flags||r->style.font_size_sp||r->style.indent_sp))goto done;
        if(values[2]&&(!mulib_docir_uri_(values[2])||
           (r->kind!=mulib_docir_LINK&&r->kind!=mulib_docir_REFERENCE&&r->kind!=mulib_docir_CITE))) {
            error=mulib_docir_E_REFERENCE;goto done;
        }
        if((r->kind==mulib_docir_REFERENCE||r->kind==mulib_docir_CITE)&&(!values[2]||values[2][0]!='#')) {
            error=mulib_docir_E_REFERENCE;goto done;
        }
        if(r->kind==mulib_docir_BIBITEM&&!values[1]){error=mulib_docir_E_REFERENCE;goto done;}
        if(values[3]&&(!*values[3]||r->source_begin>r->source_end||!r->source_line||!r->source_column))goto done;
        if(!values[3]&&(r->source_begin||r->source_end||r->source_line||r->source_column))goto done;
        if((r->kind==mulib_docir_TEXT||r->kind==mulib_docir_CODE||r->kind==mulib_docir_MATH_SYMBOL||
            r->kind==mulib_docir_REFERENCE||r->kind==mulib_docir_CITE||r->kind==mulib_docir_LINEBREAK)&&r->child_count)goto done;
        if(r->kind==mulib_docir_MATH_SYMBOL&&(!values[0]||!*values[0]))goto done;
        if(r->kind==mulib_docir_BIBLIOGRAPHY&&!r->child_count)goto done;
        if((r->kind==mulib_docir_LIST||r->kind==mulib_docir_LINEBREAK)&&values[0]&&*values[0])goto done;
        if((r->kind==mulib_docir_MATH&&r->child_count!=1)||
           ((r->kind==mulib_docir_MATH_FRACTION||r->kind==mulib_docir_MATH_SUPERSCRIPT||r->kind==mulib_docir_MATH_SUBSCRIPT)&&r->child_count!=2)||
           (r->kind==mulib_docir_MATH_RADICAL&&r->child_count!=1))goto done;
        if((r->kind==mulib_docir_MATH||r->kind==mulib_docir_MATH_ROW||
            r->kind==mulib_docir_MATH_FRACTION||r->kind==mulib_docir_MATH_SUPERSCRIPT||
            r->kind==mulib_docir_MATH_SUBSCRIPT||r->kind==mulib_docir_MATH_RADICAL)&&values[0]&&*values[0])goto done;
    }
    for(index=0;index<s->count;index++)if(children[index].children!=s->records[index].child_count)goto done;
    qsort(ids,id_count,sizeof(*ids),mulib_docir_id_compare_);
    for(size_t i=1;i<id_count;i++)if(!strcmp(ids[i-1].id,ids[i].id)) {
        index=ids[i].node;error=mulib_docir_E_REFERENCE;goto done;
    }
    if(flags&mulib_docir_CHECK_REFERENCES)for(index=0;index<s->count;index++) {
        const mulib_docir_record_t *r=s->records+index;
        const char *target=mulib_docir_snapshot_string(s,r->target);
        if(target&&*target=='#') {
            mulib_docir_id_entry_ key={target+1,0,0};
            const mulib_docir_id_entry_ *found=(const mulib_docir_id_entry_*)bsearch(&key,ids,id_count,sizeof(*ids),mulib_docir_id_compare_);
            if(!found||(r->kind==mulib_docir_CITE&&found->kind!=mulib_docir_BIBITEM)) {
                error=mulib_docir_E_REFERENCE;goto done;
            }
        }
    }
    error=mulib_docir_OK;index=SIZE_MAX;
done:MULIB_DOCIR_FREE(children);MULIB_DOCIR_FREE(ids);return mulib_docir_report_(report,error,index);
}
mulib_docir_node_t *mulib_docir_build(const mulib_docir_snapshot_t *s,unsigned flags,mulib_docir_report_t *report) {
    if(flags&~mulib_docir_NORMALIZE_TEXT){mulib_docir_report_(report,mulib_docir_E_ARGUMENT,0);return NULL;}
    if(!mulib_docir_snapshot_validate(s,0,report))return NULL;
    mulib_docir_node_t **nodes=(mulib_docir_node_t**)MULIB_DOCIR_CALLOC(s->count,sizeof(*nodes));
    if(!nodes){mulib_docir_report_(report,mulib_docir_E_MEMORY,0);return NULL;}
    size_t index=0;mulib_docir_node_t *root=NULL;
    for(;index<s->count;index++) {
        const mulib_docir_record_t *r=s->records+index;
        const char *text=mulib_docir_snapshot_string(s,r->text),*id=mulib_docir_snapshot_string(s,r->id);
        const char *target=mulib_docir_snapshot_string(s,r->target),*source=mulib_docir_snapshot_string(s,r->source);
        if((flags&mulib_docir_NORMALIZE_TEXT)&&index&&r->kind==mulib_docir_TEXT&&!id&&!target&&!source&&
           s->records[index-1].kind==mulib_docir_TEXT&&s->records[index-1].parent==r->parent&&
           s->records[index-1].id==SIZE_MAX&&s->records[index-1].target==SIZE_MAX&&s->records[index-1].source==SIZE_MAX&&
           s->records[index-1].style.flags==r->style.flags&&
           s->records[index-1].style.font_size_sp==r->style.font_size_sp&&s->records[index-1].style.indent_sp==r->style.indent_sp) {
            nodes[index]=nodes[index-1];
            if(text&&!mulib_docir_text_append(nodes[index],text,strlen(text)))goto fail;
            continue;
        }
        nodes[index]=mulib_docir_new(r->kind,text);if(!nodes[index])goto fail;
        if(!index)root=nodes[index];
        if(index&&!mulib_docir_append(nodes[r->parent],nodes[index])) {
            mulib_docir_destroy(nodes[index]);nodes[index]=NULL;goto fail;
        }
        if(!mulib_docir_set_style(nodes[index],&r->style)||
           ((r->kind==mulib_docir_HEADING||r->kind==mulib_docir_SECTION)&&!mulib_docir_set_level(nodes[index],r->level))||
           (id&&!mulib_docir_set_id(nodes[index],id))||(target&&!mulib_docir_set_target(nodes[index],target))||
           (source&&!mulib_docir_set_source(nodes[index],source,r->source_begin,r->source_end,r->source_line,r->source_column)))goto fail;
    }
    MULIB_DOCIR_FREE(nodes);mulib_docir_report_(report,mulib_docir_OK,SIZE_MAX);return root;
fail:
    mulib_docir_destroy(root);MULIB_DOCIR_FREE(nodes);mulib_docir_report_(report,mulib_docir_E_MEMORY,index);return NULL;
}
mulib_docir_node_t *mulib_docir_clone(const mulib_docir_node_t *node,const mulib_docir_limits_t *limits,
                                    unsigned flags,mulib_docir_report_t *report) {
    mulib_docir_snapshot_t snapshot={0};
    if(!mulib_docir_snapshot_make(node,limits,&snapshot,report))return NULL;
    mulib_docir_node_t *result=mulib_docir_build(&snapshot,flags,report);
    mulib_docir_snapshot_destroy(&snapshot);return result;
}
