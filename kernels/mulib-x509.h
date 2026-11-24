/** @file mulib-x509.h
 * @brief Bounded DER X.509 server certificate verification and PEM trust stores.
 * Define MULIB_IMPL_X509, SIGNATURE, DER, SHA, ENTROPY, BASE64, INET6/4/PARSEC.
 * Leaf-first chains, SAN identities, explicit or system PEM trust anchors. */
#ifndef MULIB_X509_H
#define MULIB_X509_H
#include "mulib-signature.h"
#include "mulib-base64.h"
#include "mulib-inet6.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_x509_certificate {
  mulib_der_value_t der, tbs, issuer, subject, san, signature;
  mulib_public_key_t public_key;
  int64_t not_before, not_after;
  unsigned signature_scheme, ca, key_usage, has_key_usage, server_auth, has_eku;
  int path_length;
} mulib_x509_certificate_t;
typedef struct mulib_x509_store {
  mulib_x509_certificate_t *certificates;
  unsigned char **storage;
  size_t count;
} mulib_x509_store_t;
/** Borrow canonical DER (maximum 64 KiB). Unsupported critical extensions,
 * name constraints, policy constraints and unsupported algorithms fail closed.
 * Output unchanged on failure; returns 1/0. */
int mulib_x509_parse (const void *, size_t, mulib_x509_certificate_t *);
/** DNS SAN only (no CN fallback); ASCII A-label host or numeric IP. Wildcard
 * matches one entire leftmost DNS label, with >=2 suffix labels. */
int mulib_x509_hostname (const mulib_x509_certificate_t *, const char *host);
/** Store must be zero initialized. Append supported PEM certificates from a
 * bounded file; returns 1 if at least one appended, 0 on failure (atomic).
 * Unsupported certificates in mixed bundles are skipped. Maximum 512 anchors. */
int mulib_x509_store_file (mulib_x509_store_t *, const char *path);
/** Load first available supported system CA bundle; no environment overrides. */
int mulib_x509_store_system (mulib_x509_store_t *);
void mulib_x509_store_free (mulib_x509_store_t *);
/** Verify leaf-first supplied chain (1..10 certificates), signatures, validity,
 * CA/path constraints, key usage/EKU, trust and SAN. now is Unix UTC seconds.
 * No network retrieval, revocation service or CN fallback. */
int mulib_x509_verify (const mulib_x509_certificate_t *chain, size_t count,
    const mulib_x509_store_t *, const char *host, int64_t now);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_X509) && !defined(MULIB_X509_IMPLEMENTED)
#define MULIB_X509_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static int mulib_x509_oid_ (const mulib_der_value_t *v, const char *p, size_t n)
{ return v->tag==6 && v->length==n && !memcmp (v->data,p,n); }
static int mulib_x509_eq_ (const mulib_der_value_t *a,const mulib_der_value_t *b)
{ return a->encoded_length==b->encoded_length && !memcmp (a->encoded,b->encoded,a->encoded_length); }
static int mulib_x509_hashalg_ (const mulib_der_value_t *v)
{
  mulib_der_cursor_t c=mulib_der_contents (v); mulib_der_value_t oid,p;
  if (v->tag!=0x30||!mulib_der_take (&c,6,&oid)||oid.length!=9||memcmp (oid.data,"\x60\x86\x48\x01\x65\x03\x04\x02",8)) return 0;
  if (c.offset<c.length && (!mulib_der_take (&c,5,&p)||p.length)) return 0;
  return c.offset==c.length && oid.data[8]>=1 && oid.data[8]<=3 ? oid.data[8] : 0;
}
static unsigned mulib_x509_algorithm_ (const mulib_der_value_t *v)
{
  mulib_der_cursor_t c=mulib_der_contents (v); mulib_der_value_t oid,p;
  if (v->tag!=0x30||!mulib_der_take (&c,6,&oid)) return 0;
  if (oid.length==9&&!memcmp (oid.data,"\x2a\x86\x48\x86\xf7\x0d\x01\x01",8)) {
    unsigned last=oid.data[8];
    if (last>=11&&last<=13) {
      if (c.offset<c.length&&(!mulib_der_take (&c,5,&p)||p.length)) return 0;
      return c.offset==c.length ? ((last-7)<<8)|1 : 0;
    }
    if (last==10) {
      int hash=0,mgf=0; uint32_t salt=20; unsigned seen=0;
      if (!mulib_der_take (&c,0x30,&p)||c.offset!=c.length) return 0;
      c=mulib_der_contents (&p);
      while (c.offset<c.length) {
        mulib_der_cursor_t inner; mulib_der_value_t field,a,b;
        if (mulib_der_next (&c,&field)!=1||field.tag<0xa0||field.tag>0xa2||(seen&&(field.tag<=seen))) return 0;
        seen=field.tag; inner=mulib_der_contents (&field);
        if (field.tag==0xa2) { if (!mulib_der_take (&inner,2,&a)||!mulib_der_u32 (&a,&salt)) return 0; }
        else {
          if (!mulib_der_take (&inner,0x30,&a)) return 0;
          if (field.tag==0xa0) hash=mulib_x509_hashalg_ (&a);
          else {
            mulib_der_cursor_t mc=mulib_der_contents (&a);
            if (!mulib_der_take (&mc,6,&b)||!mulib_x509_oid_ (&b,"\x2a\x86\x48\x86\xf7\x0d\x01\x01\x08",9)||!mulib_der_take (&mc,0x30,&b)||mc.offset!=mc.length) return 0;
            mgf=mulib_x509_hashalg_ (&b);
          }
        }
        if (inner.offset!=inner.length) return 0;
      }
      return hash&&hash==mgf&&salt==(hash==1?32u:hash==2?48u:64u) ? 0x0803u+(unsigned)hash : 0;
    }
  }
  if (oid.length==8&&!memcmp (oid.data,"\x2a\x86\x48\xce\x3d\x04\x03",7)&&c.offset==c.length) {
    if (oid.data[7]==2) return 0x0403;
    if (oid.data[7]==3) return 0x0503;
  }
  return 0;
}
static int mulib_x509_spki_ (const mulib_der_value_t *v,mulib_public_key_t *key)
{
  mulib_der_cursor_t c=mulib_der_contents (v),a; mulib_der_value_t alg,oid,param,bits,seq,n,e;
  if (!mulib_der_take (&c,0x30,&alg)||!mulib_der_take (&c,3,&bits)||c.offset!=c.length||bits.length<2||bits.data[0]) return 0;
  a=mulib_der_contents (&alg); if (!mulib_der_take (&a,6,&oid)) return 0;
  if (mulib_x509_oid_ (&oid,"\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01",9)) {
    if (a.offset<a.length&&(!mulib_der_take (&a,5,&param)||param.length)) return 0;
    if (a.offset!=a.length) return 0;
    c.data=bits.data+1;c.length=bits.length-1;c.offset=0;
    if (!mulib_der_take (&c,0x30,&seq)||c.offset!=c.length) return 0;
    c=mulib_der_contents (&seq);
    if (!mulib_der_take (&c,2,&n)||!mulib_der_take (&c,2,&e)||c.offset!=c.length
        ||!mulib_der_unsigned (&n,&key->key,&key->key_length)||!mulib_der_unsigned (&e,&key->exponent,&key->exponent_length)) return 0;
    key->kind=1; return key->key_length>=256&&key->key_length<=512&&key->exponent_length<=4;
  }
  if (!mulib_x509_oid_ (&oid,"\x2a\x86\x48\xce\x3d\x02\x01",7)||!mulib_der_take (&a,6,&param)||a.offset!=a.length) return 0;
  if (mulib_x509_oid_ (&param,"\x2a\x86\x48\xce\x3d\x03\x01\x07",8)) key->kind=23;
  else if (mulib_x509_oid_ (&param,"\x2b\x81\x04\x00\x22",5)) key->kind=24;
  else return 0;
  key->key=bits.data+1;key->key_length=bits.length-1;
  return key->key[0]==4&&key->key_length==(key->kind==23?65u:97u);
}
static int mulib_x509_time_ (const mulib_der_value_t *v,int64_t *out)
{
  unsigned parts[6]={0},year,month,day; size_t offset=0;
  static const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  int64_t total=0;
  if (!((v->tag==23&&v->length==13)||(v->tag==24&&v->length==15))||v->data[v->length-1]!='Z') return 0;
  for (unsigned i=0;i<6;++i) {
    unsigned digits=i==0&&v->tag==24?4:2;
    while (digits--) { unsigned ch=v->data[offset++];if(ch<'0'||ch>'9')return 0;parts[i]=parts[i]*10+ch-'0'; }
  }
  year=parts[0];if(v->tag==23)year+=year<50?2000:1900;
  month=parts[1];day=parts[2];
  if (year<1950||year>9999||month<1||month>12||!day||day>days[month-1]+(month==2&&year%4==0&&(year%100!=0||year%400==0))||parts[3]>23||parts[4]>59||parts[5]>59) return 0;
  for (unsigned y=1950;y<year;++y) total+=365+(y%4==0&&(y%100!=0||y%400==0));
  total-=7305; /* 1950-01-01 relative to 1970-01-01 */
  for (unsigned m=1;m<month;++m)total+=days[m-1]+(m==2&&year%4==0&&(year%100!=0||year%400==0));
  *out=(total+day-1)*86400+parts[3]*3600+parts[4]*60+parts[5];return 1;
}
static int mulib_x509_name_ (const mulib_der_value_t *v)
{
  mulib_der_cursor_t c=mulib_der_contents (v);mulib_der_value_t set,seq,oid,value;
  while(c.offset<c.length) {
    mulib_der_cursor_t s;
    if(!mulib_der_take(&c,0x31,&set)||!set.length)return 0;
    s=mulib_der_contents(&set);
    while(s.offset<s.length) {
      mulib_der_cursor_t a;
      if(!mulib_der_take(&s,0x30,&seq))return 0;
      a=mulib_der_contents(&seq);
      if(!mulib_der_take(&a,6,&oid)||!oid.length||mulib_der_next(&a,&value)!=1||a.offset!=a.length)return 0;
    }
  }
  return 1;
}
static int mulib_x509_extensions_ (mulib_der_cursor_t c,mulib_x509_certificate_t *cert)
{
  mulib_der_value_t seq,ext,oid,val,flag,seen[64];size_t count=0;
  if(!mulib_der_take(&c,0x30,&seq)||c.offset!=c.length)return 0;
  c=mulib_der_contents(&seq);
  while(c.offset<c.length) {
    mulib_der_cursor_t e,d;unsigned critical=0,id=0;
    if(count==64||!mulib_der_take(&c,0x30,&ext))return 0;
    e=mulib_der_contents(&ext);
    if(!mulib_der_take(&e,6,&oid))return 0;
    for(size_t i=0;i<count;++i)if(mulib_x509_eq_(&seen[i],&oid))return 0;
    seen[count++]=oid;
    if(e.offset<e.length&&e.data[e.offset]==1) {
      if(!mulib_der_take(&e,1,&flag)||flag.length!=1||flag.data[0]!=255)return 0;
      critical=1;
    }
    if(!mulib_der_take(&e,4,&val)||e.offset!=e.length)return 0;
    d=mulib_der_contents(&val);
    if(oid.length==3&&oid.data[0]==0x55&&oid.data[1]==0x1d)id=oid.data[2];
    if(id==19) {
      if(!mulib_der_take(&d,0x30,&seq)||d.offset!=d.length)return 0;
      d=mulib_der_contents(&seq);
      if(d.offset<d.length&&d.data[d.offset]==1) {
        if(!mulib_der_take(&d,1,&flag)||flag.length!=1||flag.data[0]!=255)return 0;
        cert->ca=1;
      }
      if(d.offset<d.length) {uint32_t n;if(!cert->ca||!mulib_der_take(&d,2,&flag)||!mulib_der_u32(&flag,&n)||n>100)return 0;cert->path_length=(int)n;}
      if(d.offset!=d.length)return 0;
    } else if(id==15) {
      if(!mulib_der_take(&d,3,&flag)||d.offset!=d.length||flag.length<2||flag.length>3||flag.data[0]>7
          ||(flag.data[flag.length-1]&((1u<<flag.data[0])-1)))return 0;
      cert->has_key_usage=1;cert->key_usage=flag.data[1];
    } else if(id==17) {
      mulib_der_cursor_t names;
      if(!mulib_der_take(&d,0x30,&cert->san)||d.offset!=d.length||!cert->san.length)return 0;
      names=mulib_der_contents(&cert->san);
      while(names.offset<names.length) {
        if(mulib_der_next(&names,&flag)!=1||!(flag.tag&128))return 0;
        if(flag.tag==0x87&&flag.length!=4&&flag.length!=16)return 0;
        if(flag.tag==0x82)for(size_t i=0;i<flag.length;++i)if(!flag.data[i]||flag.data[i]>127)return 0;
      }
    } else if(id==37) {
      if(!mulib_der_take(&d,0x30,&seq)||d.offset!=d.length||!seq.length)return 0;
      d=mulib_der_contents(&seq);cert->has_eku=1;
      while(d.offset<d.length) {
        if(!mulib_der_take(&d,6,&flag))return 0;
        if(mulib_x509_oid_(&flag,"\x2b\x06\x01\x05\x05\x07\x03\x01",8)||mulib_x509_oid_(&flag,"\x55\x1d\x25\x00",4))cert->server_auth=1;
      }
    } else if(id==30||id==36||id==54||critical) return 0;
  }
  return 1;
}
int mulib_x509_parse (const void *data,size_t n,mulib_x509_certificate_t *out)
{
  mulib_x509_certificate_t cert;mulib_der_cursor_t c={(const unsigned char *)data,n,0},t,v;
  mulib_der_value_t outer,inner,field,validity,spki;uint32_t version=0;
  const unsigned char *serial;size_t serialn;
  if(!data||!out||n>65536)return 0;
  memset(&cert,0,sizeof cert);cert.path_length=-1;
  if(!mulib_der_take(&c,0x30,&cert.der)||c.offset!=c.length)return 0;
  c=mulib_der_contents(&cert.der);
  if(!mulib_der_take(&c,0x30,&cert.tbs)||!mulib_der_take(&c,0x30,&outer)||!mulib_der_take(&c,3,&cert.signature)||c.offset!=c.length||cert.signature.length<2||cert.signature.data[0])return 0;
  t=mulib_der_contents(&cert.tbs);
  if(t.offset<t.length&&t.data[t.offset]==0xa0) {
    if(!mulib_der_take(&t,0xa0,&field))return 0;
    v=mulib_der_contents(&field);
    if(!mulib_der_take(&v,2,&field)||!mulib_der_u32(&field,&version)||!version||version>2||v.offset!=v.length)return 0;
  }
  if(!mulib_der_take(&t,2,&field)||!mulib_der_unsigned(&field,&serial,&serialn)||serialn>20
      ||!mulib_der_take(&t,0x30,&inner)||!mulib_x509_eq_(&inner,&outer)
      ||!mulib_der_take(&t,0x30,&cert.issuer)||!mulib_x509_name_(&cert.issuer)
      ||!mulib_der_take(&t,0x30,&validity)||!mulib_der_take(&t,0x30,&cert.subject)||!mulib_x509_name_(&cert.subject)
      ||!mulib_der_take(&t,0x30,&spki)||!mulib_x509_spki_(&spki,&cert.public_key))return 0;
  cert.signature_scheme=mulib_x509_algorithm_(&outer);if(!cert.signature_scheme)return 0;
  v=mulib_der_contents(&validity);
  if(mulib_der_next(&v,&field)!=1||!mulib_x509_time_(&field,&cert.not_before)||mulib_der_next(&v,&field)!=1
      ||!mulib_x509_time_(&field,&cert.not_after)||v.offset!=v.length||cert.not_before>cert.not_after)return 0;
  if(t.offset<t.length) {
    if(version!=2||!mulib_der_take(&t,0xa3,&field)||!mulib_x509_extensions_(mulib_der_contents(&field),&cert))return 0;
  }
  if(t.offset!=t.length)return 0;
  *out=cert;return 1;
}
static int mulib_x509_dns_ (const unsigned char *p,size_t n)
{
  size_t label=0;if(!n||n>253)return 0;
  for(size_t i=0;i<n;++i) {
    unsigned ch=p[i];
    if(ch=='.') {if(!label||label>63||p[i-1]=='-')return 0;label=0;}
    else {if(!((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='-')||(!label&&ch=='-'))return 0;++label;}
  }
  return label&&label<=63&&p[n-1]!='-';
}
int mulib_x509_hostname (const mulib_x509_certificate_t *cert,const char *host)
{
  mulib_inet4_t ip4;mulib_inet6_t ip6;unsigned char ip[16];size_t ipn=0,hn;
  mulib_der_cursor_t c;mulib_der_value_t name;
  if(!cert||!host)return 0;
  hn=strlen(host);if(!hn||hn>253)return 0;
  if(mulib_inet4_parse(host,hn,&ip4)) {memcpy(ip,ip4.bytes,4);ipn=4;}
  else if(mulib_inet6_parse(host,hn,&ip6)) {memcpy(ip,ip6.bytes,16);ipn=16;}
  else if(!mulib_x509_dns_((const unsigned char *)host,hn))return 0;
  c=mulib_der_contents(&cert->san);
  while(mulib_der_next(&c,&name)==1) {
    const unsigned char *p=name.data;size_t n=name.length,skip=0;
    if(ipn) {if(name.tag==0x87&&n==ipn&&!memcmp(p,ip,n))return 1;continue;}
    if(name.tag!=0x82)continue;
    if(n>2&&p[0]=='*'&&p[1]=='.') {
      size_t dots=0;for(size_t i=2;i<n;++i)dots+=p[i]=='.';
      if(!dots||!mulib_x509_dns_(p+2,n-2))continue;
      while(skip<hn&&host[skip]!='.')++skip;
      if(!skip||skip==hn)continue;
      ++skip;p+=2;n-=2;
    } else if(!mulib_x509_dns_(p,n))continue;
    if(n==hn-skip) {size_t i;for(i=0;i<n;++i) {unsigned a=p[i],b=(unsigned char)host[skip+i];if(a>='A'&&a<='Z')a+=32;if(b>='A'&&b<='Z')b+=32;if(a!=b)break;}if(i==n)return 1;}
  }
  return 0;
}
void mulib_x509_store_free (mulib_x509_store_t *s)
{ if(s) {for(size_t i=0;i<s->count;++i)free(s->storage[i]);free(s->storage);free(s->certificates);memset(s,0,sizeof *s);} }
int mulib_x509_store_file (mulib_x509_store_t *s,const char *path)
{
  FILE *f;char *text=NULL,*compact=NULL,*p,*end;long length;size_t added=0;
  mulib_x509_store_t temp={NULL,NULL,0};int ok=0;
  if(!s||!path||s->count>512)return 0;
  f=fopen(path,"rb");if(!f)return 0;
  if(fseek(f,0,SEEK_END)||(length=ftell(f))<=0||length>16*1024*1024||fseek(f,0,SEEK_SET))goto done;
  text=(char *)malloc((size_t)length+1);compact=(char *)malloc(90000);
  temp.certificates=(mulib_x509_certificate_t *)calloc(512,sizeof *temp.certificates);
  temp.storage=(unsigned char **)calloc(512,sizeof *temp.storage);
  if(!text||!compact||!temp.certificates||!temp.storage||fread(text,1,(size_t)length,f)!=(size_t)length)goto done;
  text[length]=0;if(memchr(text,0,(size_t)length))goto done;
  p=text;
  while((p=strstr(p,"-----BEGIN CERTIFICATE-----"))!=NULL) {
    unsigned char *der;size_t cn=0,dn=0;mulib_x509_certificate_t cert;
    p+=27;end=strstr(p,"-----END CERTIFICATE-----");if(!end)goto done;
    while(p<end) {unsigned char ch=(unsigned char)*p++;if(ch==' '||ch=='\r'||ch=='\n'||ch=='\t')continue;if(cn==90000)goto done;compact[cn++]=(char)ch;}
    p=end+25;der=(unsigned char *)malloc(65536);if(!der)goto done;
    if(!mulib_base64_decode(compact,cn,der,65536,&dn)) {free(der);goto done;}
    if(!mulib_x509_parse(der,dn,&cert)) {free(der);continue;}
    if(temp.count+s->count==512) {free(der);goto done;}
    temp.storage[temp.count]=der;temp.certificates[temp.count++]=cert;
  }
  added=temp.count;if(!added)goto done;
  memmove(temp.storage+s->count,temp.storage,added*sizeof *temp.storage);
  memmove(temp.certificates+s->count,temp.certificates,added*sizeof *temp.certificates);
  if(s->count) {memcpy(temp.storage,s->storage,s->count*sizeof *temp.storage);memcpy(temp.certificates,s->certificates,s->count*sizeof *temp.certificates);}
  temp.count+=s->count;free(s->storage);free(s->certificates);*s=temp;memset(&temp,0,sizeof temp);ok=1;
done:
  fclose(f);free(text);free(compact);mulib_x509_store_free(&temp);return ok;
}
int mulib_x509_store_system (mulib_x509_store_t *s)
{
  static const char *paths[]={"/etc/ssl/certs/ca-certificates.crt","/etc/pki/tls/certs/ca-bundle.crt","/etc/ssl/cert.pem","/etc/ssl/ca-bundle.pem"};
  for(size_t i=0;i<sizeof paths/sizeof paths[0];++i)if(mulib_x509_store_file(s,paths[i]))return 1;
  return 0;
}
static int mulib_x509_valid_ (const mulib_x509_certificate_t *c,int64_t now)
{ return now>=c->not_before&&now<=c->not_after&&(!c->has_eku||c->server_auth); }
static int mulib_x509_issuer_ (const mulib_x509_certificate_t *c,const mulib_x509_certificate_t *issuer,unsigned below,int64_t now)
{
  return issuer->ca&&mulib_x509_valid_(issuer,now)&&(!issuer->has_key_usage||(issuer->key_usage&4))
      &&(issuer->path_length<0||below<=(unsigned)issuer->path_length)&&mulib_x509_eq_(&c->issuer,&issuer->subject)
      &&mulib_signature_verify(&issuer->public_key,c->signature_scheme,c->tbs.encoded,c->tbs.encoded_length,c->signature.data+1,c->signature.length-1);
}
int mulib_x509_verify (const mulib_x509_certificate_t *chain,size_t n,const mulib_x509_store_t *store,const char *host,int64_t now)
{
  unsigned below=0;
  if(!chain||!n||n>10||!store||!store->count||!mulib_x509_hostname(chain,host)
      ||chain[0].ca||(chain[0].has_key_usage&&!(chain[0].key_usage&128)))return 0;
  for(size_t i=0;i<n;++i) {
    const mulib_x509_certificate_t *c=chain+i;
    if(!mulib_x509_valid_(c,now))return 0;
    for(size_t j=0;j<store->count;++j) {
      const mulib_x509_certificate_t *root=store->certificates+j;
      if(mulib_x509_eq_(&c->der,&root->der)&&mulib_x509_valid_(root,now))return 1;
      if(mulib_x509_issuer_(c,root,below+(i>0),now))return 1;
    }
    if(i+1==n||!mulib_x509_issuer_(c,chain+i+1,below+(i>0),now))return 0;
    below+=(i>0);
  }
  return 0;
}
#endif
