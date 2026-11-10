int mulib_epub_write_docir(const mulib_epub_book_t *metadata,const mulib_epub_docir_chapter_t *chapters,
                           size_t count,mulib_buffer_t *out) {
#ifdef MULIB_ENABLE_DOCIR
    mulib_typecore_buf_t resource_memory={0},text_memory={0};int rc=MULIB_EPUB_ENOMEM;
    if(!metadata||!chapters||!count||count>4096||!out||(!metadata->resources&&metadata->resource_count)||
       metadata->resource_count>SIZE_MAX-count||metadata->resource_count+count>SIZE_MAX/sizeof(mulib_epub_resource_t)||
       count>SIZE_MAX/sizeof(mulib_strbuf_t))return MULIB_EPUB_EINVAL;
    size_t total=metadata->resource_count+count;
    if(!mulib_typecore_reserve(&resource_memory,total*sizeof(mulib_epub_resource_t))||
       !mulib_typecore_reserve(&text_memory,count*sizeof(mulib_strbuf_t)))goto done;
    mulib_epub_resource_t *resources=(mulib_epub_resource_t*)resource_memory.data;
    mulib_strbuf_t *texts=(mulib_strbuf_t*)text_memory.data;
    memset(texts,0,count*sizeof(*texts));
    for(size_t i=0;i<metadata->resource_count;i++)resources[i]=metadata->resources[i];
    for(size_t i=0;i<count;i++) {
        if(!chapters[i].path||!chapters[i].title||!chapters[i].root||
           !mulib_html_docir(chapters[i].root,chapters[i].title,metadata->language,texts+i)) {
            rc=MULIB_EPUB_EINVAL;goto text_done;
        }
        /* The shared XML reader deliberately rejects all DTDs. The HTML
         * adapter's fixed HTML5 doctype is unnecessary in the XHTML resource. */
        if(texts[i].length<16||memcmp(texts[i].data,"<!DOCTYPE html>\n",16)) {rc=MULIB_EPUB_EFORMAT;goto text_done;}
        resources[metadata->resource_count+i]=(mulib_epub_resource_t){chapters[i].path,"application/xhtml+xml",
            chapters[i].title,strstr(texts[i].data,"<math ")?"mathml":NULL,texts[i].data+16,texts[i].length-16};
    }
    mulib_epub_book_t book=*metadata;book.resources=resources;book.resource_count=total;
    rc=mulib_epub_write(&book,out);
text_done:for(size_t i=0;i<count;i++)mulib_strbuf_free(texts+i);
done:mulib_typecore_free(&text_memory);mulib_typecore_free(&resource_memory);return rc;
#else
    (void)metadata;(void)chapters;(void)count;(void)out;return MULIB_EPUB_EUNSUPPORTED;
#endif
}
