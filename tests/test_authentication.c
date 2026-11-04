#define MULIB_IMPL_AUTHCRYPTO
#define MULIB_IMPL_BASE64
#define MULIB_IMPL_SECURITY
#define MULIB_IMPL_AUTH
#define MULIB_IMPL_OAUTH2
#define MULIB_IMPL_OAUTH
#define MULIB_IMPL_OTPAUTH
#define MULIB_IMPL_OTPATH
#define MULIB_IMPL_PWDAUTH
#define MULIB_IMPL_ACL
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INET4
#define MULIB_IMPL_PARSEC
#include "mulib-oauth.h"
#include "mulib-otpath.h"
#include "mulib-pwdauth.h"
#include "mulib-acl.h"
#include <assert.h>
#include <stdio.h>

static void
hex_equal (const unsigned char *bytes, size_t n, const char *hex)
{
  const char alphabet[]="0123456789abcdef";
  assert(strlen(hex)==n*2);
  for (size_t i=0; i<n; ++i) {
    assert(hex[i*2]==alphabet[bytes[i]>>4]);
    assert(hex[i*2+1]==alphabet[bytes[i]&15]);
  }
}
static void
crypto (void)
{
  unsigned char out[32], key[131];
  const char *long_message="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  memset(out,0x5a,sizeof out);
  assert(!mulib_authcrypto_hash((mulib_authcrypto_algorithm)0,"abc",3,out,32));
  assert(!mulib_authcrypto_hash(MULIB_AUTHCRYPTO_SHA256,NULL,1,out,32));
  assert(!mulib_authcrypto_hash(MULIB_AUTHCRYPTO_SHA256,"abc",3,out,31));
  assert(out[0]==0x5a && out[31]==0x5a);
  assert(mulib_authcrypto_hash(MULIB_AUTHCRYPTO_SHA1,"abc",3,out,32));
  hex_equal(out,20,"a9993e364706816aba3e25717850c26c9cd0d89d");
  assert(mulib_authcrypto_hash(MULIB_AUTHCRYPTO_SHA256,"abc",3,out,32));
  hex_equal(out,32,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  assert(mulib_authcrypto_hash(MULIB_AUTHCRYPTO_SHA1,NULL,0,out,32));
  hex_equal(out,20,"da39a3ee5e6b4b0d3255bfef95601890afd80709");
  assert(mulib_authcrypto_hash(MULIB_AUTHCRYPTO_SHA256,NULL,0,out,32));
  hex_equal(out,32,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  assert(mulib_authcrypto_hash(MULIB_AUTHCRYPTO_SHA1,long_message,strlen(long_message),out,32));
  hex_equal(out,20,"84983e441c3bd26ebaae4aa1f95129e5e54670f1");
  assert(mulib_authcrypto_hash(MULIB_AUTHCRYPTO_SHA256,long_message,strlen(long_message),out,32));
  hex_equal(out,32,"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  memset(key,0x0b,20);
  assert(mulib_authcrypto_hmac(MULIB_AUTHCRYPTO_SHA1,key,20,"Hi There",8,out,32));
  hex_equal(out,20,"b617318655057264e28bc0b6fb378c8ef146be00");
  assert(mulib_authcrypto_hmac(MULIB_AUTHCRYPTO_SHA256,key,20,"Hi There",8,out,32));
  hex_equal(out,32,"b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
  memset(key,0xaa,sizeof key);
  assert(mulib_authcrypto_hmac(MULIB_AUTHCRYPTO_SHA256,key,sizeof key,
    "Test Using Larger Than Block-Size Key - Hash Key First",54,out,32));
  hex_equal(out,32,"60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
  assert(mulib_authcrypto_hmac(MULIB_AUTHCRYPTO_SHA1,key,80,
    "Test Using Larger Than Block-Size Key - Hash Key First",54,out,32));
  hex_equal(out,20,"aa4ae5e15272d00e95705637ce8a3b55ed402112");
  memset(out,0xa5,sizeof out);
  assert(!mulib_authcrypto_hmac(MULIB_AUTHCRYPTO_SHA256,NULL,1,"",0,out,32));
  assert(out[0]==0xa5);
  assert(!mulib_authcrypto_pbkdf2("password",8,"salt",4,0,out));
  assert(out[0]==0xa5);
  assert(mulib_authcrypto_pbkdf2("password",8,"salt",4,1,out));
  hex_equal(out,32,"120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
  assert(mulib_authcrypto_pbkdf2("password",8,"salt",4,2,out));
  hex_equal(out,32,"ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
  assert(mulib_authcrypto_pbkdf2("password",8,"salt",4,4096,out));
  hex_equal(out,32,"c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
  mulib_authcrypto_wipe(out,sizeof out);
  for (size_t i=0; i<sizeof out; ++i) assert(!out[i]);
}
static void
otp (void)
{
  static const char *hotp[]={"755224","287082","359152","969429","338314",
                            "254676","287922","162583","399871","520489"};
  static const uint64_t times[]={59,1111111109,1111111111,1234567890,2000000000,UINT64_C(20000000000)};
  static const char *sha1[]={"94287082","07081804","14050471","89005924","69279037","65353130"};
  static const char *sha256[]={"46119246","68084774","67062674","91819424","90698825","77737706"};
  mulib_otpauth_config_t c={"12345678901234567890",20,MULIB_AUTHCRYPTO_SHA1,6,30,0};
  mulib_otpauth_replay_t replay={0,0};
  char out[9]="sentinel";
  unsigned char decoded[32];
  size_t n=99;
  uint64_t next=0;
  for (unsigned i=0; i<10; ++i) {
    assert(mulib_otpauth_hotp(&c,i,out,sizeof out));
    assert(!strcmp(out,hotp[i]));
  }
  assert(mulib_otpauth_hotp_verify(&c,hotp[3],3,&next) && next==4);
  assert(!mulib_otpauth_hotp_verify(&c,hotp[3],3,&next) && next==4);
  assert(!mulib_otpauth_hotp_verify(&c,"nototp",3,&next) && next==4);
  assert(!mulib_otpauth_hotp_verify(&c,hotp[4],101,&next));
  next=UINT64_MAX;
  assert(mulib_otpauth_hotp(&c,next,out,sizeof out));
  assert(!mulib_otpauth_hotp_verify(&c,out,1,&next) && next==UINT64_MAX);
  c.digits=8;
  for (unsigned i=0; i<6; ++i) {
    assert(mulib_otpauth_totp(&c,times[i],out,sizeof out));
    assert(!strcmp(out,sha1[i]));
  }
  c.secret="12345678901234567890123456789012"; c.secret_length=32;
  c.algorithm=MULIB_AUTHCRYPTO_SHA256;
  for (unsigned i=0; i<6; ++i) {
    assert(mulib_otpath_totp(&c,times[i],out,sizeof out));
    assert(!strcmp(out,sha256[i]));
  }
  assert(mulib_otpath_totp_verify(&c,sha256[0],59,1,1,&replay));
  assert(replay.counter==1 && replay.initialized);
  assert(!mulib_otpauth_totp_verify(&c,sha256[0],59,1,1,&replay));
  assert(!mulib_otpauth_totp_verify(&c,sha256[0],59,11,1,&replay));
  assert(!mulib_otpauth_totp_verify(&c,sha256[0],59,1,1,NULL));
  replay.initialized=2;
  assert(!mulib_otpauth_totp_verify(&c,sha256[0],59,1,1,&replay));
  replay.initialized=0;
  assert(mulib_otpauth_totp(&c,0,out,sizeof out));
  assert(mulib_otpauth_totp_verify(&c,out,0,10,0,&replay));
  assert(replay.counter==0 && replay.initialized);
  assert(!mulib_otpauth_totp_verify(&c,out,0,0,0,&replay));
  replay.initialized=0; c.period=1;
  assert(mulib_otpauth_totp(&c,UINT64_MAX,out,sizeof out));
  assert(mulib_otpauth_totp_verify(&c,out,UINT64_MAX,0,10,&replay));
  assert(replay.counter==UINT64_MAX);
  c.epoch=100;
  assert(!mulib_otpauth_totp(&c,99,out,sizeof out));
  c.period=0;
  assert(!mulib_otpauth_totp(&c,100,out,sizeof out));
  c.digits=5;
  strcpy(out,"sentinel");
  assert(!mulib_otpauth_hotp(&c,0,out,sizeof out) && !strcmp(out,"sentinel"));
  c.digits=8; c.secret_length=15;
  assert(!mulib_otpauth_hotp(&c,0,out,sizeof out));
  c.secret_length=32;
  assert(!mulib_otpauth_hotp(&c,0,out,8));
  assert(mulib_otpauth_base32("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ",32,decoded,32,&n));
  assert(n==20 && !memcmp(decoded,"12345678901234567890",20));
  {
    static const char *encoded[]={"MY======","MZXQ====","MZXW6===","MZXW6YQ=","MZXW6YTB","MZXW6YTBOI======"};
    static const char *plain[]={"f","fo","foo","foob","fooba","foobar"};
    for (unsigned i=0; i<6; ++i) {
      assert(mulib_otpauth_base32(encoded[i],strlen(encoded[i]),decoded,32,&n));
      assert(n==strlen(plain[i]) && !memcmp(decoded,plain[i],n));
      assert(mulib_otpauth_base32(encoded[i],strcspn(encoded[i],"="),decoded,32,&n));
    }
  }
  assert(mulib_otpauth_base32("my",2,decoded,32,&n) && n==1 && decoded[0]=='f');
  {
    static const char *bad[]={"","M","MZ","MY=","MY=======","MY====== ","M=Y=====","MZXW6YTA=","MZXW6YTB0I","MY\n"};
    memset(decoded,0xaa,sizeof decoded); n=99;
    for (unsigned i=0; i<sizeof bad/sizeof *bad; ++i) {
      assert(!mulib_otpauth_base32(bad[i],strlen(bad[i]),decoded,32,&n));
      assert(n==99 && decoded[0]==0xaa);
    }
    assert(!mulib_otpauth_base32("MY",2,decoded,0,&n));
  }
}
static int calls, verdict;
static int
basic_verify (void *ctx, const char *user, size_t un, const char *pass, size_t pn)
{
  assert(ctx==&calls); ++calls;
  assert(un==1 && user[0]=='a' && pn==3 && !memcmp(pass,"b:c",3));
  return verdict;
}
static int
token_verify (void *ctx, const char *token)
{
  assert(ctx==&calls); ++calls; assert(!strcmp(token,"abc")); return verdict;
}
static int
oauth_verify (void *ctx, const char *token, const char *scope)
{
  assert(!strcmp(scope,"read")); return token_verify(ctx,token);
}
static int
claims_verify (void *ctx, const char *token, mulib_oauth2_claims_t *claims)
{
  claims->issuer="https://issuer"; claims->audience="service";
  claims->scope="read write"; claims->not_before=10; claims->expires_at=20;
  return token_verify(ctx,token);
}
static void
http_auth (void)
{
  char out[32]="unchanged";
  size_t n=99;
  const char basic_bytes[]={'B','a','s','i','c',' ','Y','T','p','i'};
  const char *bad[]={"Basic YQ==","Basic YQBiOmM=","Basic YToK","Basic YTpi=",
                     "Basic YTpi\n","Basic\tYTpi","Basic ","Other YTpi",NULL};
  assert(mulib_auth_basic_n(basic_bytes,sizeof basic_bytes,out,sizeof out,&n));
  assert(n==3 && !strcmp(out,"a:b"));
  assert(mulib_auth_basic("bAsIc   YTpi",out,sizeof out,&n));
  for (unsigned i=0; i<sizeof bad/sizeof *bad; ++i) {
    strcpy(out,"unchanged"); n=99;
    assert(!mulib_auth_basic(bad[i],out,sizeof out,&n));
    assert(n==99 && !strcmp(out,"unchanged"));
  }
  assert(!mulib_auth_basic("Basic YTpi",out,3,&n));
  assert(mulib_auth_bearer("bEaReR   abc"));
  assert(!mulib_auth_bearer("Bearer ="));
  assert(!mulib_auth_bearer("Bearer abc def"));
  assert(!mulib_auth_bearer("Bearer a=b"));
  calls=0; verdict=1;
  assert(mulib_auth_basic_authorize("Basic YTpiOmM=",basic_verify,&calls) && calls==1);
  assert(!mulib_auth_basic_authorize("Basic YQ==",basic_verify,&calls) && calls==1);
  verdict=2;
  assert(!mulib_auth_basic_authorize("Basic YTpiOmM=",basic_verify,&calls));
  assert(!mulib_auth_bearer_authorize("Bearer abc",token_verify,&calls));
  verdict=1;
  assert(mulib_auth_bearer_authorize("Bearer abc",token_verify,&calls));
  assert(!mulib_auth_bearer_authorize("Bearer abc",NULL,&calls));
  assert(mulib_auth_apikey("a\0b",3,"a\0b",3));
  assert(!mulib_auth_apikey("a\0b",3,"a\0c",3));
  assert(!mulib_auth_apikey("a",1,"ab",2));
  assert(!mulib_auth_apikey("",0,"",0));
  assert(!mulib_auth_apikey(NULL,1,"a",1));
  {
    char *large=(char *)malloc(MULIB_AUTH_MAX_HEADER+2);
    assert(large);
    memset(large,'a',MULIB_AUTH_MAX_HEADER+1);
    memcpy(large,"Bearer ",7); large[MULIB_AUTH_MAX_HEADER+1]=0;
    assert(!mulib_auth_bearer(large));
    memcpy(large,"Basic ",6);
    assert(!mulib_auth_basic(large,out,sizeof out,&n));
    free(large);
  }
}
static void
oauth (void)
{
  const char *verifier="dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
  const char *challenge="E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM";
  char out[44]="unchanged";
  mulib_oauth2_transaction_t t={"session-bound-state",100,0};
  mulib_oauth2_claims_t c={"https://issuer","service","read write",20,10};
  assert(mulib_oauth_pkce_challenge(verifier,out,sizeof out) && !strcmp(out,challenge));
  assert(mulib_oauth2_pkce_verify(verifier,challenge));
  assert(!mulib_oauth2_pkce_verify(verifier,"plain"));
  strcpy(out,"unchanged");
  assert(!mulib_oauth2_pkce_challenge("short",out,sizeof out) && !strcmp(out,"unchanged"));
  assert(!mulib_oauth2_pkce_challenge(verifier,out,43));
  assert(!mulib_oauth2_pkce_challenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjX!",out,44));
  assert(mulib_oauth2_scope("read write other","write read"));
  assert(!mulib_oauth2_scope("read:all","read"));
  assert(!mulib_oauth2_scope("read","Read"));
  assert(!mulib_oauth2_scope("read  write","read"));
  assert(!mulib_oauth2_scope("read","read write"));
  assert(!mulib_oauth2_scope("read",""));
  assert(!mulib_oauth2_scope("read","read\n"));
  assert(!mulib_oauth2_scope("read","read\\"));
  assert(mulib_oauth_state("state","state") && !mulib_oauth_state("",""));
  assert(!mulib_oauth2_state_consume(&t,"wrong",50) && !t.consumed);
  assert(!mulib_oauth2_state_consume(&t,t.state,100) && !t.consumed);
  assert(mulib_oauth2_state_consume(&t,t.state,99) && t.consumed);
  assert(!mulib_oauth2_state_consume(&t,t.state,99));
  assert(mulib_oauth2_claims_check(&c,"https://issuer","service","read",10));
  assert(!mulib_oauth2_claims_check(&c,"https://issuer","service","read",9));
  assert(!mulib_oauth2_claims_check(&c,"https://issuer","service","read",20));
  assert(!mulib_oauth2_claims_check(&c,"https://Issuer","service","read",15));
  assert(!mulib_oauth2_claims_check(&c,"https://issuer","other","read",15));
  assert(!mulib_oauth2_claims_check(&c,"https://issuer","service","admin",15));
  calls=0; verdict=1;
  assert(mulib_oauth_authorize("Bearer abc","read",oauth_verify,&calls));
  assert(!mulib_oauth2_authorize("Bearer abc","read",NULL,&calls));
  assert(!mulib_oauth2_authorize("Bearer abc","read\n",oauth_verify,&calls));
  assert(mulib_oauth2_authorize_claims("Bearer abc","https://issuer","service","read",15,claims_verify,&calls));
  assert(!mulib_oauth2_authorize_claims("Bearer abc","https://issuer","other","read",15,claims_verify,&calls));
  assert(!mulib_oauth2_authorize_claims("Bearer abc","https://issuer","service","read",20,claims_verify,&calls));
  verdict=2;
  assert(!mulib_oauth2_authorize_claims("Bearer abc","https://issuer","service","read",15,claims_verify,&calls));
  assert(!mulib_oauth_authorize("Bearer abc","read",oauth_verify,&calls));
}
static void
acl (void)
{
  mulib_acl_rule_t r[2], saved;
  mulib_acl6_rule_t r6[2], saved6;
  mulib_inet6_t ip;
  size_t matched=0;
  assert(mulib_acl_rule_parse("10.1.2.3/8",1,&r[0]));
  assert(r[0].network==0x0a000000 && r[0].prefix==8);
  assert(mulib_acl_rule_parse("0.0.0.0/0",0,&r[1]));
  assert(mulib_acl_check_ex(r,2,0x0a010203,0,&matched) && matched==0);
  assert(!mulib_acl_check_ex(r,2,0x7f000001,1,&matched) && matched==1);
  assert(mulib_acl_check_ex(NULL,0,0,1,&matched) && matched==SIZE_MAX);
  r[1].prefix=33;
  assert(!mulib_acl_check_ex(r,2,0x0a010203,1,&matched) && matched==SIZE_MAX);
  assert(!mulib_acl_validate(NULL,1));
  assert(!mulib_acl_check(NULL,0,0,2));
  saved=r[0];
  {
    const char *bad[]={"10.0.0.1","10.0.0.1/33","10.0.0.1/-1","10.0.0.1/08",
                        "010.0.0.1/8","10.0.0.1/8x","10.0.0.1/8/1","10.0.0.1/"};
    for (unsigned i=0; i<sizeof bad/sizeof *bad; ++i) {
      assert(!mulib_acl_rule_parse(bad[i],1,&r[0]));
      assert(r[0].network==saved.network && r[0].prefix==saved.prefix);
    }
  }
  assert(mulib_acl_rule_parse("255.255.255.255/32",1,&r[0]));
  assert(mulib_acl_check(r,1,UINT32_MAX,0) && !mulib_acl_check(r,1,0,0));
  assert(mulib_acl6_rule_parse("2001:db8:1234::1/32",1,&r6[0]));
  assert(mulib_acl6_rule_parse("::/0",0,&r6[1]));
  assert(mulib_inet6_parse("2001:db8::42",12,&ip));
  assert(mulib_acl6_check(r6,2,&ip,0,&matched) && matched==0);
  assert(mulib_inet6_parse("::1",3,&ip));
  assert(!mulib_acl6_check(r6,2,&ip,1,&matched) && matched==1);
  saved6=r6[0];
  assert(!mulib_acl6_rule_parse("2001:db8::/129",1,&r6[0]));
  assert(!memcmp(&saved6,&r6[0],sizeof saved6));
  assert(!mulib_acl6_rule_parse("fe80::1%eth0/64",1,&r6[0]));
  r6[1].allow=2;
  assert(!mulib_acl6_check(r6,2,&ip,1,&matched) && matched==SIZE_MAX);
  assert(!mulib_acl6_check(NULL,0,NULL,1,&matched));
  assert(mulib_acl6_check(NULL,0,&ip,1,&matched));
}
static void
passwords (void)
{
  /* Deterministic salt ONLY for a test vector; production needs a CSPRNG. */
  const unsigned char salt[]="0123456789abcdef";
  mulib_pwdauth_record_t r;
  memset(&r,0,sizeof r);
  assert(!mulib_pwdauth_create("password",8,salt,1,&r) && r.iterations==0);
  assert(!mulib_pwdauth_create(NULL,1,salt,MULIB_PWDAUTH_MIN_ITERATIONS,&r));
  assert(mulib_pwdauth_create("pass\0word",9,salt,MULIB_PWDAUTH_MIN_ITERATIONS,&r));
  assert(mulib_pwdauth_verify(&r,"pass\0word",9));
  assert(!mulib_pwdauth_verify(&r,"pass",4));
  r.iterations=UINT32_MAX;
  assert(!mulib_pwdauth_verify(&r,"pass\0word",9));
  mulib_authcrypto_wipe(&r,sizeof r);
}
int main (void)
{
  crypto(); otp(); http_auth(); oauth(); acl(); passwords();
  puts("authentication: RFC vectors, policy failures, replay and boundaries passed");
  return 0;
}
