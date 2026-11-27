LIB = asdl.lib.new("on1x.source.policy")
asdl.lib.version(LIB, "1")
error("compiler must not load a signed library from LIBSTONE_ASDL_PATH")
asdl.signlib(LIB)
return LIB
