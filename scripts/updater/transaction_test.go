package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"io"
	"os"
	"path"
	"path/filepath"
	"reflect"
	"runtime"
	"strings"
	"testing"
	"time"

	"asterbound/updater/internal/release"
)

func openTestRoot(t *testing.T) *os.Root {
	t.Helper()
	r, err := os.OpenRoot(t.TempDir())
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { r.Close() })
	return r
}

func putFile(t *testing.T, root *os.Root, name, value string) {
	t.Helper()
	if err := root.MkdirAll(path.Dir(name), 0700); err != nil {
		t.Fatal(err)
	}
	if err := root.WriteFile(name, []byte(value), 0700); err != nil {
		t.Fatal(err)
	}
}

func expectFile(t *testing.T, root *os.Root, name, value string) {
	t.Helper()
	data, err := root.ReadFile(name)
	if err != nil || string(data) != value {
		t.Fatalf("read %s = %q, %v; want %q", name, data, err, value)
	}
}

func TestInstallHandshake(t *testing.T) {
	for _, cmd := range []string{"COMMIT\n", "", "COMMIT", "CANCEL\n", "COMMIT\r\n", "garbage"} {
		in, w, err := os.Pipe()
		if err != nil {
			t.Fatal(err)
		}
		out, ready := io.Pipe()
		control, ctx, cancel := monitorClient(in)
		done := make(chan error, 1)
		go func() {
			done <- control.requestCommit(ctx, ready)
		}()
		var reply [6]byte
		if _, err := io.ReadFull(out, reply[:]); err != nil || string(reply[:]) != "READY\n" {
			t.Fatalf("unexpected readiness: %q, %v", reply, err)
		}
		if _, err := io.WriteString(w, cmd); err != nil {
			t.Fatal(err)
		}
		w.Close()
		err = <-done
		cancel()
		in.Close()
		out.Close()
		ready.Close()
		if (err == nil) != (cmd == "COMMIT\n") {
			t.Errorf("input %q: err=%v", cmd, err)
		}
	}
}

func TestDisconnectOrEarlyCommitCancelsPreparation(t *testing.T) {
	for _, input := range []string{"", "CANCEL\n", "COMMIT\n"} {
		control, ctx, cancel := monitorClient(strings.NewReader(input))
		select {
		case <-ctx.Done():
		case <-time.After(time.Second):
			t.Fatal("control pipe did not cancel the preparation context")
		}
		var out bytes.Buffer
		if err := control.requestCommit(ctx, &out); err == nil || out.Len() != 0 {
			t.Fatalf("input %q after cancellation: err=%v, output=%q", input, err, out.String())
		}
		cancel()
	}
}

func TestTransactionRollback(t *testing.T) {
	for _, failure := range []string{"partial replacement", "launch"} {
		t.Run(failure, func(t *testing.T) {
			r := openTestRoot(t)
			client := release.ClientPath(runtime.GOOS)
			files := []string{"assets/asterbound.pak", "assets/asterbound-compat.pak", client}
			putFile(t, r, files[0], "old asset")
			putFile(t, r, client, "old client")
			for _, p := range files {
				putFile(t, r, staged(p), "not an executable")
			}
			for _, p := range []string{"assets/packed.pak", "mods/personal.pak", "storage/player/test.player"} {
				putFile(t, r, p, "user data")
			}
			plan, err := preflight(r, files)
			if err != nil {
				t.Fatal(err)
			}
			if failure == "partial replacement" {
				if err := r.Remove(staged(client)); err != nil {
					t.Fatal(err)
				}
			}
			attempted := false
			keep, err := apply(r, plan, func() error {
				attempted = true
				return startClient(filepath.Join(r.Name(), filepath.FromSlash(client)), nil)
			})
			if keep || err == nil || attempted != (failure == "launch") {
				t.Fatalf("retain backups=%v, err=%v, attempted launch=%v", keep, err, attempted)
			}
			expectFile(t, r, files[0], "old asset")
			expectFile(t, r, client, "old client")
			if _, err := r.Lstat(files[1]); !errors.Is(err, os.ErrNotExist) {
				t.Fatalf("newly added file survived rollback: %v", err)
			}
			for _, p := range []string{"assets/packed.pak", "mods/personal.pak", "storage/player/test.player"} {
				expectFile(t, r, p, "user data")
			}
		})
	}
}

func TestRollbackConflictRetainsOriginal(t *testing.T) {
	r := openTestRoot(t)
	p := "assets/asterbound.pak"
	putFile(t, r, p, "old")
	putFile(t, r, staged(p), "new")
	plan, err := preflight(r, []string{p})
	if err != nil {
		t.Fatal(err)
	}
	keep, err := apply(r, plan, func() error {
		putFile(t, r, p, "concurrent change")
		return errors.New("cannot launch")
	})
	if !keep || err == nil {
		t.Fatalf("retain backups=%v, err=%v", keep, err)
	}
	expectFile(t, r, p, "concurrent change")
	expectFile(t, r, backup(p), "old")
}

func TestRejectRedirectedDestination(t *testing.T) {
	r := openTestRoot(t)
	p := "assets/asterbound.pak"
	putFile(t, r, staged(p), "replacement")
	outside := t.TempDir()
	if err := os.Symlink(outside, filepath.Join(r.Name(), "assets")); err != nil {
		if runtime.GOOS == "windows" {
			t.Skipf("symlink creation unavailable: %v", err)
		}
		t.Fatal(err)
	}
	if _, err := preflight(r, []string{p}); err == nil {
		t.Fatal("accepted linked destination directory")
	}
	if contents, err := os.ReadDir(outside); err != nil || len(contents) != 0 {
		t.Fatalf("modified redirected destination: %v, %v", contents, err)
	}
	if err := r.Remove("assets"); err != nil {
		t.Fatal(err)
	}
	putFile(t, r, "mods/custom.pak", "mine")
	if err := r.Mkdir("assets", 0700); err != nil {
		t.Fatal(err)
	}
	if err := r.Symlink("../mods/custom.pak", p); err != nil {
		t.Fatal(err)
	}
	if _, err := preflight(r, []string{p}); err == nil {
		t.Fatal("accepted a linked file inside the install root")
	}
	expectFile(t, r, "mods/custom.pak", "mine")
}

// Invoked as an actual child process by the relaunch test below. The marker
// is published only after all bytes have been written.
func TestRelaunchedClient(t *testing.T) {
	marker := os.Getenv("ASTERBOUND_TEST_RELAUNCH_MARKER")
	if marker == "" {
		return
	}
	cwd, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	data, err := json.Marshal(append([]string{cwd}, os.Args[1:]...))
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(marker+".tmp", data, 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.Rename(marker+".tmp", marker); err != nil {
		t.Fatal(err)
	}
}

func TestInstallAndRelaunchPreservesInvocation(t *testing.T) {
	r := openTestRoot(t)
	client := release.ClientPath(runtime.GOOS)
	putFile(t, r, client, "old client")
	putFile(t, r, staged(client), "")
	exe, err := os.Executable()
	if err != nil {
		t.Fatal(err)
	}
	in, err := os.Open(exe)
	if err != nil {
		t.Fatal(err)
	}
	defer in.Close()
	out, err := r.OpenFile(staged(client), os.O_WRONLY, 0)
	if err != nil {
		t.Fatal(err)
	}
	_, err = io.Copy(out, in)
	if err := errors.Join(err, out.Close()); err != nil {
		t.Fatal(err)
	}
	plan, err := preflight(r, []string{client})
	if err != nil {
		t.Fatal(err)
	}
	cwd := t.TempDir()
	t.Chdir(cwd)
	marker := filepath.Join(cwd, "relaunch.json")
	t.Setenv("ASTERBOUND_TEST_RELAUNCH_MARKER", marker)
	args := []string{"-test.run=^TestRelaunchedClient$", "--", "value with spaces", "", `a\"b`, `--config=custom`, "; echo no-shell"}
	keep, err := apply(r, plan, func() error {
		return startClient(filepath.Join(r.Name(), filepath.FromSlash(client)), args)
	})
	if keep || err != nil {
		t.Fatalf("retain backups=%v, err=%v", keep, err)
	}
	deadline := time.Now().Add(10 * time.Second)
	var data []byte
	for {
		data, err = os.ReadFile(marker)
		if err == nil {
			break
		}
		if !errors.Is(err, os.ErrNotExist) || time.Now().After(deadline) {
			t.Fatal(err)
		}
		time.Sleep(20 * time.Millisecond)
	}
	var got []string
	if err := json.Unmarshal(data, &got); err != nil {
		t.Fatal(err)
	}
	actualCwd, err := os.Getwd()
	if err != nil || !reflect.DeepEqual(got, append([]string{actualCwd}, args...)) {
		t.Fatalf("unexpected relaunched cwd/arguments: %q (%v)", got, err)
	}
}
