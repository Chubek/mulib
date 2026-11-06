module mull.stdlib.Stdlib;
import MuDSL;
import mull.stdlib.printf.Native;
import mull.stdlib.terminal.Native;
import mull.stdlib.math.Native;
import mull.stdlib.text.Native;
import mull.stdlib.list.Native;

void install(MuLLRuntime runtime)
{
    installPrintf(runtime); installTerminal(runtime); installMath(runtime);
    installText(runtime); installList(runtime);
}
