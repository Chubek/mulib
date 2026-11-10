static int mulib_docir_sp_(mulib_typecore_buf_t *out,int32_t sp) {
    char text[48];int64_t value=sp;const char *sign="";if(value<0){sign="-";value=-value;}
    unsigned whole=(unsigned)(value/65536),fraction=(unsigned)((value%65536)*1000000/65536);
    int n=snprintf(text,sizeof(text),"%s%u.%06upt",sign,whole,fraction);
    return n>=0&&(size_t)n<sizeof(text)&&mulib_typecore_append(out,text,(size_t)n);
}
static int mulib_docir_styled_(const mulib_docir_record_t *r) {
    return r->style.flags||r->style.font_size_sp||r->style.indent_sp;
}
static const char *mulib_docir_html_tag_(const mulib_docir_snapshot_t *s,const mulib_docir_record_t *r,char heading[3]) {
    switch(r->kind) {
    case mulib_docir_ROOT:return r->id!=SIZE_MAX||mulib_docir_styled_(r)?"div":NULL;
    case mulib_docir_SECTION:return "section";
    case mulib_docir_PARAGRAPH:return "p";
    case mulib_docir_TEXT:return r->id!=SIZE_MAX||mulib_docir_styled_(r)?"span":NULL;
    case mulib_docir_LINK:return r->target==SIZE_MAX?"span":"a";
    case mulib_docir_LIST:return "ul";
    case mulib_docir_ITEM:case mulib_docir_BIBITEM:return "li";
    case mulib_docir_HEADING:heading[0]='h';heading[1]=(char)('0'+r->level);heading[2]=0;return heading;
    case mulib_docir_MATH:return "math";
    case mulib_docir_MATH_ROW:return "mrow";
    case mulib_docir_MATH_SYMBOL: {
        const char *text=mulib_docir_snapshot_string(s,r->text);
        if(text&&*text&&strspn(text,"0123456789")==strlen(text))return "mn";
        if(text&&text[0]&&!text[1]&&strchr("+-=*/()[]<>|!,:;",text[0]))return "mo";
        return "mi";
    }
    case mulib_docir_MATH_FRACTION:return "mfrac";
    case mulib_docir_MATH_SUPERSCRIPT:return "msup";
    case mulib_docir_MATH_SUBSCRIPT:return "msub";
    case mulib_docir_MATH_RADICAL:return "msqrt";
    case mulib_docir_REFERENCE:case mulib_docir_CITE:return "a";
    case mulib_docir_FIGURE:return "figure";
    case mulib_docir_CAPTION:return "figcaption";
    case mulib_docir_FOOTNOTE:return "span";
    case mulib_docir_BIBLIOGRAPHY:return "section";
    case mulib_docir_LINEBREAK:return "br";
    case mulib_docir_CODE:return "code";
    default:return NULL;
    }
}
static int mulib_docir_html_open_(const mulib_docir_snapshot_t *s,const mulib_docir_record_t *r,mulib_typecore_buf_t *out) {
    char heading[3];const char *tag=mulib_docir_html_tag_(s,r,heading);
    const char *text=mulib_docir_snapshot_string(s,r->text),*id=mulib_docir_snapshot_string(s,r->id);
    const char *target=mulib_docir_snapshot_string(s,r->target);
    if(tag) {
        if(!mulib_typecore_byte(out,'<')||!mulib_typecore_text(out,tag))return 0;
        if(r->kind==mulib_docir_MATH&&!mulib_typecore_text(out," xmlns=\"http://www.w3.org/1998/Math/MathML\""))return 0;
        if(id&&(!mulib_typecore_text(out," id=\"")||!mulib_typecore_html(out,id,strlen(id))||!mulib_typecore_byte(out,'"')))return 0;
        if(target&&(!mulib_typecore_text(out," href=\"")||!mulib_typecore_html(out,target,strlen(target))||!mulib_typecore_byte(out,'"')))return 0;
        if(r->kind==mulib_docir_FOOTNOTE&&!mulib_typecore_text(out," role=\"doc-footnote\""))return 0;
        if(r->kind==mulib_docir_BIBLIOGRAPHY&&!mulib_typecore_text(out," role=\"doc-bibliography\""))return 0;
        if(r->kind==mulib_docir_CITE&&!mulib_typecore_text(out," role=\"doc-biblioref\""))return 0;
        if(mulib_docir_styled_(r)) {
            if(!mulib_typecore_text(out," style=\""))return 0;
            if((r->style.flags&mulib_docir_BOLD)&&!mulib_typecore_text(out,"font-weight:bold;"))return 0;
            if((r->style.flags&mulib_docir_ITALIC)&&!mulib_typecore_text(out,"font-style:italic;"))return 0;
            if((r->style.flags&mulib_docir_MONOSPACE)&&!mulib_typecore_text(out,"font-family:monospace;"))return 0;
            if(r->style.font_size_sp&&(!mulib_typecore_text(out,"font-size:")||!mulib_docir_sp_(out,r->style.font_size_sp)||!mulib_typecore_byte(out,';')))return 0;
            if(r->style.indent_sp&&(!mulib_typecore_text(out,"margin-inline-start:")||!mulib_docir_sp_(out,r->style.indent_sp)||!mulib_typecore_byte(out,';')))return 0;
            if(!mulib_typecore_byte(out,'"'))return 0;
        }
        if(!mulib_typecore_text(out,r->kind==mulib_docir_LINEBREAK?" />":">"))return 0;
    }
    if(r->kind==mulib_docir_BIBLIOGRAPHY) {
        if(text&&(!mulib_typecore_text(out,"<h2>")||!mulib_typecore_html(out,text,strlen(text))||!mulib_typecore_text(out,"</h2>")))return 0;
        return mulib_typecore_text(out,"<ol>");
    }
    if((r->kind==mulib_docir_REFERENCE||r->kind==mulib_docir_CITE)&&!text)text=target+1;
    return !text||mulib_typecore_html(out,text,strlen(text));
}
static int mulib_docir_html_close_(const mulib_docir_snapshot_t *s,const mulib_docir_record_t *r,mulib_typecore_buf_t *out) {
    char heading[3];const char *tag=mulib_docir_html_tag_(s,r,heading);
    if(r->kind==mulib_docir_LINEBREAK||!tag)return 1;
    if(r->kind==mulib_docir_BIBLIOGRAPHY&&!mulib_typecore_text(out,"</ol>"))return 0;
    return mulib_typecore_text(out,"</")&&mulib_typecore_text(out,tag)&&mulib_typecore_byte(out,'>');
}
static int mulib_docir_latex_math_(mulib_typecore_buf_t *out,const char *text) {
    size_t length=strlen(text);
    if((strspn(text,"0123456789.")==length)||
       (length==1&&strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz+-=*/()[]<>|!,:;",text[0])))
        return mulib_typecore_text(out,text);
    if(strspn(text,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz")==length)
        return mulib_typecore_text(out,"\\mathrm{")&&mulib_typecore_text(out,text)&&mulib_typecore_byte(out,'}');
    static const struct { const char *utf8,*tex; } symbols[]={
        {"\xce\xb1","\\alpha"},{"\xce\xb2","\\beta"},{"\xce\xb3","\\gamma"},
        {"\xce\xb4","\\delta"},{"\xcf\x80","\\pi"},{"\xcf\x83","\\sigma"},
        {"\xce\xa3","\\Sigma"},{"\xe2\x88\x9e","\\infty"},{"\xe2\x89\xa4","\\leq"},
        {"\xe2\x89\xa5","\\geq"},{"\xe2\x89\xa0","\\neq"},{"\xc3\x97","\\times"}};
    for(size_t i=0;i<sizeof(symbols)/sizeof(symbols[0]);i++)if(!strcmp(text,symbols[i].utf8))
        return mulib_typecore_text(out,symbols[i].tex)&&mulib_typecore_text(out,"{}");
    return mulib_typecore_text(out,"\\text{")&&mulib_typecore_latex(out,text,strlen(text))&&mulib_typecore_byte(out,'}');
}
static int mulib_docir_latex_anchor_(mulib_typecore_buf_t *out,const char *id) {
    return !id||(mulib_typecore_text(out,"\\hypertarget{")&&mulib_typecore_text(out,id)&&mulib_typecore_text(out,"}{}"));
}
static int mulib_docir_latex_open_(const mulib_docir_snapshot_t *s,size_t index,mulib_typecore_buf_t *out) {
    const mulib_docir_record_t *r=s->records+index;
    const char *text=mulib_docir_snapshot_string(s,r->text),*id=mulib_docir_snapshot_string(s,r->id);
    const char *target=mulib_docir_snapshot_string(s,r->target);
    if(r->parent!=SIZE_MAX&&index!=r->parent+1) {
        unsigned parent=s->records[r->parent].kind;
        if(parent==mulib_docir_MATH_FRACTION&&!mulib_typecore_text(out,"}{"))return 0;
        if(parent==mulib_docir_MATH_SUPERSCRIPT&&!mulib_typecore_text(out,"}^{"))return 0;
        if(parent==mulib_docir_MATH_SUBSCRIPT&&!mulib_typecore_text(out,"}_{"))return 0;
    }
    if(mulib_docir_styled_(r)) {
        if(!mulib_typecore_byte(out,'{'))return 0;
        if(r->style.font_size_sp&&(!mulib_typecore_text(out,"\\fontsize{")||!mulib_docir_sp_(out,r->style.font_size_sp)||
           !mulib_typecore_text(out,"}{")||!mulib_docir_sp_(out,(int32_t)((int64_t)r->style.font_size_sp*6/5))||
           !mulib_typecore_text(out,"}\\selectfont ")))return 0;
        if((r->style.flags&mulib_docir_BOLD)&&!mulib_typecore_text(out,"\\bfseries "))return 0;
        if((r->style.flags&mulib_docir_ITALIC)&&!mulib_typecore_text(out,"\\itshape "))return 0;
        if((r->style.flags&mulib_docir_MONOSPACE)&&!mulib_typecore_text(out,"\\ttfamily "))return 0;
        if(r->style.indent_sp&&(!mulib_typecore_text(out,"\\hspace*{")||!mulib_docir_sp_(out,r->style.indent_sp)||!mulib_typecore_byte(out,'}')))return 0;
    }
    if((r->kind==mulib_docir_MATH||r->kind==mulib_docir_BIBLIOGRAPHY||
        r->kind==mulib_docir_HEADING||r->kind==mulib_docir_CAPTION)&&!mulib_docir_latex_anchor_(out,id))return 0;
    switch(r->kind) {
    case mulib_docir_SECTION:
        if(text) {
            const char *commands[]={"\\section{","\\subsection{","\\subsubsection{","\\paragraph{","\\subparagraph{","\\subparagraph{"};
            if(!mulib_typecore_text(out,commands[r->level-1])||!mulib_typecore_latex(out,text,strlen(text))||!mulib_typecore_text(out,"}\n"))return 0;
        }
        text=NULL;break;
    case mulib_docir_HEADING: {
        const char *commands[]={"\\section{","\\subsection{","\\subsubsection{","\\paragraph{","\\subparagraph{","\\subparagraph{"};
        if(!mulib_typecore_text(out,commands[r->level-1]))return 0;
        break;
    }
    case mulib_docir_LINK:case mulib_docir_REFERENCE:
        if(target) {
            if(!mulib_typecore_text(out,*target=='#'?"\\hyperlink{":"\\href{")||
               (*target=='#'?!mulib_typecore_text(out,target+1):!mulib_typecore_latex(out,target,strlen(target)))||
               !mulib_typecore_text(out,"}{"))return 0;
        } else if(!mulib_typecore_byte(out,'{'))return 0;
        if(!text&&r->kind==mulib_docir_REFERENCE)text=target+1;
        break;
    case mulib_docir_LIST:if(r->child_count&&!mulib_typecore_text(out,"\\begin{itemize}\n"))return 0;break;
    case mulib_docir_ITEM:if(!mulib_typecore_text(out,"\\item "))return 0;break;
    case mulib_docir_MATH:if(!mulib_typecore_text(out,"\\("))return 0;break;
    case mulib_docir_MATH_ROW:if(!mulib_typecore_byte(out,'{'))return 0;break;
    case mulib_docir_MATH_SYMBOL:return mulib_docir_latex_math_(out,text);
    case mulib_docir_MATH_FRACTION:if(!mulib_typecore_text(out,"\\frac{"))return 0;break;
    case mulib_docir_MATH_SUPERSCRIPT:case mulib_docir_MATH_SUBSCRIPT:if(!mulib_typecore_byte(out,'{'))return 0;break;
    case mulib_docir_MATH_RADICAL:if(!mulib_typecore_text(out,"\\sqrt{"))return 0;break;
    case mulib_docir_FIGURE:if(!mulib_typecore_text(out,"\\begin{figure}[htbp]\n\\centering\n"))return 0;break;
    case mulib_docir_CAPTION:if(!mulib_typecore_text(out,"\\caption{"))return 0;break;
    case mulib_docir_FOOTNOTE:if(!mulib_typecore_text(out,"\\footnote{"))return 0;break;
    case mulib_docir_BIBLIOGRAPHY:
        if(!mulib_typecore_byte(out,'{'))return 0;
        if(text&&(!mulib_typecore_text(out,"\\renewcommand{\\refname}{")||!mulib_typecore_latex(out,text,strlen(text))||!mulib_typecore_text(out,"}\n")))return 0;
        if(!mulib_typecore_text(out,"\\begin{thebibliography}{999}\n"))return 0;
        text=NULL;break;
    case mulib_docir_BIBITEM:
        if(!mulib_typecore_text(out,"\\bibitem{")||!mulib_typecore_text(out,id)||!mulib_typecore_text(out,"} "))return 0;
        break;
    case mulib_docir_CITE:
        if(text) {
            if(!mulib_typecore_text(out,"\\hyperlink{")||!mulib_typecore_text(out,target+1)||!mulib_typecore_text(out,"}{")||
               !mulib_typecore_latex(out,text,strlen(text))||!mulib_typecore_byte(out,'}'))return 0;
        } else if(!mulib_typecore_text(out,"\\cite{")||!mulib_typecore_text(out,target+1)||!mulib_typecore_byte(out,'}'))return 0;
        text=NULL;break;
    case mulib_docir_LINEBREAK:if(!mulib_typecore_text(out,"\\newline{}"))return 0;text=NULL;break;
    case mulib_docir_CODE:if(!mulib_typecore_text(out,"\\texttt{"))return 0;break;
    default:break;
    }
    if(r->kind!=mulib_docir_MATH&&r->kind!=mulib_docir_BIBLIOGRAPHY&&r->kind!=mulib_docir_HEADING&&
       r->kind!=mulib_docir_CAPTION&&!mulib_docir_latex_anchor_(out,id))return 0;
    return !text||mulib_typecore_latex(out,text,strlen(text));
}
static int mulib_docir_latex_close_(const mulib_docir_record_t *r,mulib_typecore_buf_t *out) {
    const char *end=NULL;
    switch(r->kind) {
    case mulib_docir_PARAGRAPH:end="\n\n";break;
    case mulib_docir_HEADING:end="}\n";break;
    case mulib_docir_LINK:case mulib_docir_REFERENCE:case mulib_docir_CAPTION:case mulib_docir_FOOTNOTE:
    case mulib_docir_CODE:case mulib_docir_MATH_ROW:case mulib_docir_MATH_FRACTION:
    case mulib_docir_MATH_SUPERSCRIPT:case mulib_docir_MATH_SUBSCRIPT:case mulib_docir_MATH_RADICAL:end="}";break;
    case mulib_docir_LIST:end=r->child_count?"\\end{itemize}\n":NULL;break;
    case mulib_docir_ITEM:case mulib_docir_BIBITEM:end="\n";break;
    case mulib_docir_MATH:end="\\)";break;
    case mulib_docir_FIGURE:end="\n\\end{figure}\n";break;
    case mulib_docir_BIBLIOGRAPHY:end="\\end{thebibliography}}\n";break;
    default:break;
    }
    return (!end||mulib_typecore_text(out,end))&&(!mulib_docir_styled_(r)||mulib_typecore_byte(out,'}'));
}
int mulib_docir_render_snapshot(const mulib_docir_snapshot_t *snapshot,unsigned format,mulib_typecore_buf_t *out) {
    mulib_typecore_buf_t result={0};size_t ancestors[4097],depth=0;int ok=0;
    if(!out||format>1||!mulib_docir_snapshot_validate(snapshot,0,NULL))return 0;
    mulib_docir_snapshot_t s=*snapshot;int latex=format==mulib_docir_FORMAT_LATEX;
    for(size_t i=0;i<s.count;i++) {
        while(depth>s.records[i].depth) {
            const mulib_docir_record_t *r=s.records+ancestors[--depth];
            if(latex?!mulib_docir_latex_close_(r,&result):!mulib_docir_html_close_(&s,r,&result))goto done;
        }
        if(latex?!mulib_docir_latex_open_(&s,i,&result):!mulib_docir_html_open_(&s,s.records+i,&result))goto done;
        ancestors[depth++]=i;
    }
    while(depth) {
        const mulib_docir_record_t *r=s.records+ancestors[--depth];
        if(latex?!mulib_docir_latex_close_(r,&result):!mulib_docir_html_close_(&s,r,&result))goto done;
    }
    ok=mulib_typecore_append(out,result.data,result.size);
done:mulib_typecore_free(&result);return ok;
}
static int mulib_docir_export_(const mulib_docir_node_t *node,mulib_typecore_buf_t *out,unsigned format) {
    mulib_docir_snapshot_t snapshot={0};
    if(!out||!mulib_docir_snapshot_make(node,NULL,&snapshot,NULL))return 0;
    int ok=mulib_docir_render_snapshot(&snapshot,format,out);mulib_docir_snapshot_destroy(&snapshot);return ok;
}
int mulib_docir_html(const mulib_docir_node_t *node,mulib_typecore_buf_t *out){return mulib_docir_export_(node,out,mulib_docir_FORMAT_HTML);}
int mulib_docir_latex(const mulib_docir_node_t *node,mulib_typecore_buf_t *out){return mulib_docir_export_(node,out,mulib_docir_FORMAT_LATEX);}
