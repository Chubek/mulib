#include "Runtime.hpp"
extern "C" const mull::mu3ac::Config mu3ac_config;
extern "C" mull::mu3ac::Handle mu3ac_entry(void *, void *);
int main(int argc, char **argv) { return mull::mu3ac::nativeMain(mu3ac_config,mu3ac_entry,argc,argv); }
