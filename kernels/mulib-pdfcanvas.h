/** @file mulib-pdfcanvas.h
 * @brief Single-page PDF 1.4 document writer with Helvetica ASCII text.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_PDFCANVAS once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-pdfcanvas.md for scope and limitations.
 */
#ifndef MULIB_PDFCANVAS_H
#define MULIB_PDFCANVAS_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pdfcanvas_write_text(const char *text,double x,double y,double font_size,mulib_typecore_buf_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PDFCANVAS_H */
#if defined(MULIB_IMPL_PDFCANVAS) && !defined(MULIB_PDFCANVAS_H_IMPLEMENTED)
#define MULIB_PDFCANVAS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static int mulib_pdfcanvas_printf_(mulib_typecore_buf_t *b,const char *fmt,size_t v){char tmp[96];int n=snprintf(tmp,sizeof(tmp),fmt,v);return n>=0&&(size_t)n<sizeof(tmp)&&mulib_typecore_append(b,tmp,(size_t)n);}
int mulib_pdfcanvas_write_text(const char *text,double x,double y,double pt,mulib_typecore_buf_t *out){if(!text||!out||!isfinite(x)||!isfinite(y)||!isfinite(pt)||pt<=0||pt>1000)return 0;
 mulib_typecore_buf_t content={0},b={0};size_t offsets[6]={0};char pos[128];int k=snprintf(pos,sizeof(pos),"BT /F1 %.3f Tf %.3f %.3f Td (",pt,x,y);if(k<0||(size_t)k>=sizeof(pos))goto fail;
 if(!mulib_typecore_append(&content,pos,(size_t)k))goto fail;
 for(const unsigned char *p=(const unsigned char*)text;*p;p++){if(*p<32||*p>126)goto fail;if((*p=='('||*p==')'||*p=='\\')&&!mulib_typecore_byte(&content,'\\'))goto fail;if(!mulib_typecore_byte(&content,*p))goto fail;}
 if(!mulib_typecore_text(&content,") Tj ET\n")||!mulib_typecore_text(&b,"%PDF-1.4\n%MuLIB\n"))goto fail;
 offsets[1]=b.size;if(!mulib_typecore_text(&b,"1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n"))goto fail;
 offsets[2]=b.size;if(!mulib_typecore_text(&b,"2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n"))goto fail;
 offsets[3]=b.size;if(!mulib_typecore_text(&b,"3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >> endobj\n"))goto fail;
 offsets[4]=b.size;if(!mulib_typecore_text(&b,"4 0 obj << /Length ")||!mulib_pdfcanvas_printf_(&b,"%zu",content.size)||!mulib_typecore_text(&b," >> stream\n")||!mulib_typecore_append(&b,content.data,content.size)||!mulib_typecore_text(&b,"endstream endobj\n"))goto fail;
 offsets[5]=b.size;if(!mulib_typecore_text(&b,"5 0 obj << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >> endobj\n"))goto fail;
 size_t xref=b.size;if(!mulib_typecore_text(&b,"xref\n0 6\n0000000000 65535 f \n"))goto fail;for(size_t i=1;i<=5;i++){char line[32];int n=snprintf(line,sizeof(line),"%010zu 00000 n \n",offsets[i]);if(n!=20||!mulib_typecore_append(&b,line,(size_t)n))goto fail;}
 if(!mulib_typecore_text(&b,"trailer << /Size 6 /Root 1 0 R >>\nstartxref\n")||!mulib_pdfcanvas_printf_(&b,"%zu",xref)||!mulib_typecore_text(&b,"\n%%EOF\n")||!mulib_typecore_append(out,b.data,b.size))goto fail;
 mulib_typecore_free(&content);mulib_typecore_free(&b);return 1;
 fail:mulib_typecore_free(&content);mulib_typecore_free(&b);return 0;}
#ifdef __cplusplus
}
#endif
#endif
