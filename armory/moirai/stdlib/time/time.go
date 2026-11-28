package time

import (
	"fmt"
	"time"

	"domscript/internal/ffi"
	"domscript/internal/runtime"
)

var startTime = time.Now()

func Register(st *runtime.State) {
	reg := func(name string, fn runtime.GoNative) {
		ffi.Register(st, "__time_"+name, fn)
	}

	reg("now", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Float(float64(time.Now().UnixNano()) / 1e9), nil
	})

	reg("unix", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Int(time.Now().Unix()), nil
	})

	reg("unix_nano", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Int(time.Now().UnixNano()), nil
	})

	reg("monotonic", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Float(float64(time.Since(startTime).Nanoseconds()) / 1e9), nil
	})

	reg("sleep", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("sleep expects a duration in seconds")
		}
		d := time.Duration(args[0].Numeric() * float64(time.Second))
		time.Sleep(d)
		return runtime.Unit(), nil
	})

	reg("format", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 || args[0].Kind() != runtime.KindFloat && args[0].Kind() != runtime.KindInt {
			return runtime.Unit(), fmt.Errorf("format expects a unix timestamp and optional layout")
		}
		layout := "2006-01-02 15:04:05"
		if len(args) > 1 && args[1].Kind() == runtime.KindString {
			layout = args[1].StrValue()
		}
		t := time.Unix(int64(args[0].Numeric()), 0)
		return runtime.Str(t.Format(layout)), nil
	})

	reg("parse", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 || args[0].Kind() != runtime.KindString || args[1].Kind() != runtime.KindString {
			return runtime.Unit(), fmt.Errorf("parse expects layout and value :Strings")
		}
		t, err := time.Parse(args[0].StrValue(), args[1].StrValue())
		if err != nil {
			return runtime.TaggedOf("Error", []runtime.Value{runtime.Str(err.Error())}), nil
		}
		return runtime.TaggedOf("Success", []runtime.Value{runtime.Int(t.Unix())}), nil
	})

	reg("from_unix", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("from_unix expects a timestamp")
		}
		t := time.Unix(int64(args[0].Numeric()), 0)
		tbl := timeTable(t)
		return runtime.TableValue(tbl), nil
	})

	reg("year", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Int(int64(time.Now().Year())), nil
	})

	reg("month", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Int(int64(time.Now().Month())), nil
	})

	reg("day", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Int(int64(time.Now().Day())), nil
	})

	reg("hour", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Int(int64(time.Now().Hour())), nil
	})

	reg("minute", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Int(int64(time.Now().Minute())), nil
	})

	reg("second", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Int(int64(time.Now().Second())), nil
	})

	reg("weekday", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Str(time.Now().Weekday().String()), nil
	})

	reg("add", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 {
			return runtime.Unit(), fmt.Errorf("add expects a timestamp and duration in seconds")
		}
		t := time.Unix(int64(args[0].Numeric()), 0)
		d := time.Duration(args[1].Numeric() * float64(time.Second))
		return runtime.Int(t.Add(d).Unix()), nil
	})

	reg("sub", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 2 {
			return runtime.Unit(), fmt.Errorf("sub expects two timestamps")
		}
		a := time.Unix(int64(args[0].Numeric()), 0)
		b := time.Unix(int64(args[1].Numeric()), 0)
		return runtime.Float(a.Sub(b).Seconds()), nil
	})

	reg("since", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		if len(args) < 1 {
			return runtime.Unit(), fmt.Errorf("since expects a timestamp")
		}
		t := time.Unix(int64(args[0].Numeric()), 0)
		return runtime.Float(time.Since(t).Seconds()), nil
	})

	reg("timer", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Float(float64(time.Since(startTime).Nanoseconds()) / 1e9), nil
	})

	reg("iso8601", func(st *runtime.State, args []runtime.Value) (runtime.Value, error) {
		return runtime.Str(time.Now().Format(time.RFC3339)), nil
	})
}

func timeTable(t time.Time) *runtime.Table {
	tbl := runtime.NewTable()
	tbl.Set(runtime.Str("year"), runtime.Int(int64(t.Year())))
	tbl.Set(runtime.Str("month"), runtime.Int(int64(t.Month())))
	tbl.Set(runtime.Str("day"), runtime.Int(int64(t.Day())))
	tbl.Set(runtime.Str("hour"), runtime.Int(int64(t.Hour())))
	tbl.Set(runtime.Str("minute"), runtime.Int(int64(t.Minute())))
	tbl.Set(runtime.Str("second"), runtime.Int(int64(t.Second())))
	tbl.Set(runtime.Str("weekday"), runtime.Str(t.Weekday().String()))
	tbl.Set(runtime.Str("unix"), runtime.Int(t.Unix()))
	tbl.Set(runtime.Str("unix_nano"), runtime.Int(t.UnixNano()))
	tbl.Set(runtime.Str("iso8601"), runtime.Str(t.Format(time.RFC3339)))
	return tbl
}
