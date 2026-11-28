package process

import (
	"fmt"
	"os"
	"os/exec"
	"strings"
	"syscall"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__process_"+name, fn)
	}

	reg("run", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("run expects a :String command")
		}
		cmdParts := strings.Fields(args[0].StrValue())
		if len(cmdParts) == 0 {
			return runtime.Unit(), fmt.Errorf("run expects a non-empty command")
		}
		var cmd *exec.Cmd
		if len(cmdParts) == 1 {
			cmd = exec.Command(cmdParts[0])
		} else {
			cmd = exec.Command(cmdParts[0], cmdParts[1:]...)
		}
		out, err := cmd.CombinedOutput()
		t := runtime.NewTable()
		t.Set(runtime.Str("stdout"), runtime.Str(string(out)))
		if cmd.ProcessState != nil {
			t.Set(runtime.Str("exit_code"), runtime.Int(int64(cmd.ProcessState.ExitCode())))
		}
		if err != nil {
			t.Set(runtime.Str("error"), runtime.Str(err.Error()))
		}
		return runtime.TableValue(t), nil
	})

	reg("exec", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("exec expects a :String command")
		}
		cmdParts := strings.Fields(args[0].StrValue())
		if len(cmdParts) == 0 {
			return runtime.Unit(), fmt.Errorf("exec expects a non-empty command")
		}
		var cmd *exec.Cmd
		if len(cmdParts) == 1 {
			cmd = exec.Command(cmdParts[0])
		} else {
			cmd = exec.Command(cmdParts[0], cmdParts[1:]...)
		}
		out, err := cmd.Output()
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Str(string(out))}), nil
	})

	reg("start", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("start expects a :String command")
		}
		cmdParts := strings.Fields(args[0].StrValue())
		if len(cmdParts) == 0 {
			return runtime.Unit(), fmt.Errorf("start expects a non-empty command")
		}
		var cmd *exec.Cmd
		if len(cmdParts) == 1 {
			cmd = exec.Command(cmdParts[0])
		} else {
			cmd = exec.Command(cmdParts[0], cmdParts[1:]...)
		}
		err := cmd.Start()
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Int(int64(cmd.Process.Pid))}), nil
	})

	reg("pid", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Int(int64(os.Getpid())), nil
	})

	reg("signal", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("signal expects an :Int pid and a signal")
		}
		pid := int(args[0].IntValue())
		proc, err := os.FindProcess(pid)
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		var sig syscall.Signal
		switch {
		case args[1].Kind() == runtime.KindInt:
			sig = syscall.Signal(args[1].IntValue())
		case args[1].Kind() == runtime.KindString:
			switch strings.ToUpper(args[1].StrValue()) {
			case "HUP", "SIGHUP":
				sig = syscall.SIGHUP
			case "INT", "SIGINT":
				sig = syscall.SIGINT
			case "TERM", "SIGTERM":
				sig = syscall.SIGTERM
			case "KILL", "SIGKILL":
				sig = syscall.SIGKILL
			case "QUIT", "SIGQUIT":
				sig = syscall.SIGQUIT
			default:
				return runtime.Unit(), fmt.Errorf("signal: unknown signal %q", args[1].StrValue())
			}
		default:
			return runtime.Unit(), fmt.Errorf("signal expects a signal name or number")
		}
		if err := proc.Signal(sig); err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Unit()}), nil
	})
}
