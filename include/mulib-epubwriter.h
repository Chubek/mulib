/** @file mulib-epubwriter.h
 * @brief Minimal EPUB 3.0 writer: valid stored ZIP, mimetype, container, package, nav and one XHTML chapter.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_EPUBWRITER once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-epubwriter.md for scope and limitations.
 */
#ifndef MULIB_EPUBWRITER_H
#define MULIB_EPUBWRITER_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_epubwriter_book(const char *title,const char *body,mulib_typecore_buf_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EPUBWRITER_H */
#if defined(MULIB_IMPL_EPUBWRITER) && !defined(MULIB_EPUBWRITER_H_IMPLEMENTED)
#define MULIB_EPUBWRITER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_epubwriter_zipitem {const char *name;const void *data;size_t size;uint32_t offset,crc;} mulib_epubwriter_zipitem_t;
static int mulib_epubwriter_zip_(mulib_typecore_buf_t *out,mulib_epubwriter_zipitem_t *items,size_t n){mulib_typecore_buf_t b={0},cd={0};if(n>65535)return 0;
 for(size_t i=0;i<n;i++){const char *name=items[i].name;size_t nl=strlen(name);if(nl>65535||items[i].size>UINT32_MAX||b.size>UINT32_MAX)goto fail;items[i].offset=(uint32_t)b.size;items[i].crc=mulib_typecore_crc32(items[i].data,items[i].size);
 if(!mulib_typecore_u32le(&b,0x04034b50u)||!mulib_typecore_u16le(&b,20)||!mulib_typecore_u16le(&b,0)||!mulib_typecore_u16le(&b,0)||!mulib_typecore_u16le(&b,0)||!mulib_typecore_u16le(&b,0)||!mulib_typecore_u32le(&b,items[i].crc)||!mulib_typecore_u32le(&b,(uint32_t)items[i].size)||!mulib_typecore_u32le(&b,(uint32_t)items[i].size)||!mulib_typecore_u16le(&b,(uint16_t)nl)||!mulib_typecore_u16le(&b,0)||!mulib_typecore_append(&b,name,nl)||!mulib_typecore_append(&b,items[i].data,items[i].size))goto fail;
 if(!mulib_typecore_u32le(&cd,0x02014b50u)||!mulib_typecore_u16le(&cd,20)||!mulib_typecore_u16le(&cd,20)||!mulib_typecore_u16le(&cd,0)||!mulib_typecore_u16le(&cd,0)||!mulib_typecore_u16le(&cd,0)||!mulib_typecore_u16le(&cd,0)||!mulib_typecore_u32le(&cd,items[i].crc)||!mulib_typecore_u32le(&cd,(uint32_t)items[i].size)||!mulib_typecore_u32le(&cd,(uint32_t)items[i].size)||!mulib_typecore_u16le(&cd,(uint16_t)nl)||!mulib_typecore_u16le(&cd,0)||!mulib_typecore_u16le(&cd,0)||!mulib_typecore_u16le(&cd,0)||!mulib_typecore_u16le(&cd,0)||!mulib_typecore_u32le(&cd,0)||!mulib_typecore_u32le(&cd,items[i].offset)||!mulib_typecore_append(&cd,name,nl))goto fail;
 }
 if(b.size>UINT32_MAX||cd.size>UINT32_MAX)goto fail;
 uint32_t dir_at=(uint32_t)b.size;if(!mulib_typecore_append(&b,cd.data,cd.size)||!mulib_typecore_u32le(&b,0x06054b50u)||!mulib_typecore_u16le(&b,0)||!mulib_typecore_u16le(&b,0)||!mulib_typecore_u16le(&b,(uint16_t)n)||!mulib_typecore_u16le(&b,(uint16_t)n)||!mulib_typecore_u32le(&b,(uint32_t)cd.size)||!mulib_typecore_u32le(&b,dir_at)||!mulib_typecore_u16le(&b,0)||!mulib_typecore_append(out,b.data,b.size))goto fail;
 mulib_typecore_free(&cd);mulib_typecore_free(&b);return 1;
 fail:mulib_typecore_free(&cd);mulib_typecore_free(&b);return 0;}
int mulib_epubwriter_book(const char *title,const char *body,mulib_typecore_buf_t *out){if(!title||!body||!out)return 0;
 const char *mime="application/epub+zip";
 const char *container="<?xml version=\"1.0\" encoding=\"UTF-8\"?><container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\"><rootfiles><rootfile full-path=\"OEBPS/content.opf\" media-type=\"application/oebps-package+xml\"/></rootfiles></container>";
 const char *nav="<?xml version=\"1.0\" encoding=\"UTF-8\"?><html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\"><head><title>Contents</title></head><body><nav epub:type=\"toc\"><h1>Contents</h1><ol><li><a href=\"chapter.xhtml\">Chapter</a></li></ol></nav></body></html>";
 mulib_typecore_buf_t chapter={0},opf={0};int ok=0;
 if(!mulib_typecore_text(&chapter,"<?xml version=\"1.0\" encoding=\"UTF-8\"?><html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>")||!mulib_typecore_html(&chapter,title,strlen(title))||!mulib_typecore_text(&chapter,"</title></head><body><p>")||!mulib_typecore_html(&chapter,body,strlen(body))||!mulib_typecore_text(&chapter,"</p></body></html>"))goto done;
 if(!mulib_typecore_text(&opf,"<?xml version=\"1.0\" encoding=\"UTF-8\"?><package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\" unique-identifier=\"id\"><metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:identifier id=\"id\">urn:uuid:00000000-0000-4000-8000-000000000000</dc:identifier><dc:title>")||!mulib_typecore_html(&opf,title,strlen(title))||!mulib_typecore_text(&opf,"</dc:title><dc:language>en</dc:language><meta property=\"dcterms:modified\">2000-01-01T00:00:00Z</meta></metadata><manifest><item id=\"chapter\" href=\"chapter.xhtml\" media-type=\"application/xhtml+xml\"/><item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\" properties=\"nav\"/></manifest><spine><itemref idref=\"chapter\"/></spine></package>"))goto done;
 mulib_epubwriter_zipitem_t items[]={ {"mimetype",mime,strlen(mime),0,0}, {"META-INF/container.xml",container,strlen(container),0,0}, {"OEBPS/content.opf",opf.data,opf.size,0,0}, {"OEBPS/nav.xhtml",nav,strlen(nav),0,0}, {"OEBPS/chapter.xhtml",chapter.data,chapter.size,0,0} };ok=mulib_epubwriter_zip_(out,items,5);
 done:mulib_typecore_free(&chapter);mulib_typecore_free(&opf);return ok;}
#ifdef __cplusplus
}
#endif
#endif
