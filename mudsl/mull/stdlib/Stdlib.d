module mull.stdlib.Stdlib;
import MuDSL;
import mull.native.printf.Native;
import mull.native.terminal.Native;
import mull.native.math.Native;
import mull.native.text.Native;
import mull.native.list.Native;
version (MULL_ENABLE_MULIB) import mull.bridge.Mulib.Native;

void install(MuLLRuntime runtime)
{
    installPrintf(runtime); installTerminal(runtime); installMath(runtime);
    installText(runtime); installList(runtime);
    version (MULL_ENABLE_MULIB) installMulib(runtime);
}
