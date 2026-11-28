module mull.mu3ac.Main;
import mull.mu3ac.Runtime;
extern(C) extern __gshared Config mu3ac_config;
extern(C) Handle mu3ac_entry(void*, void*);
int main(string[] args) { return nativeMain(mu3ac_config, &mu3ac_entry, args); }
