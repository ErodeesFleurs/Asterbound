package main

import (
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
	"time"

	"asterbound/updater/internal/release"
)

func TestStageExcludesConcurrentAndInterruptedInstalls(t *testing.T) {
	r := openTestRoot(t)
	if _, err := claimStage(r); err != nil {
		t.Fatal(err)
	}
	putFile(t, r, backup("assets/asterbound.pak"), "only surviving original")
	if _, err := claimStage(r); err == nil || !strings.Contains(err.Error(), "plan.json") {
		t.Fatalf("did not fail closed with recovery instructions: %v", err)
	}
	expectFile(t, r, backup("assets/asterbound.pak"), "only surviving original")
}

func TestStandaloneInstallation(t *testing.T) {
	r := openTestRoot(t)
	client := release.ClientPath(runtime.GOOS)
	platform, _, _ := strings.Cut(client, "/")
	for _, p := range []string{client, path.Join(path.Dir(client), helperName()), path.Join(platform, "sbinit.config"), "assets/asterbound.pak"} {
		putFile(t, r, p, "fixture")
	}
	exe := filepath.Join(r.Name(), filepath.FromSlash(client))
	check := func(wantCombined bool) {
		t.Helper()
		found, rel, combined, err := installation(exe)
		if err != nil {
			t.Fatal(err)
		}
		defer found.Close()
		if rel != client || combined != wantCombined {
			t.Errorf("incorrect installation layout: %s combined=%v", rel, combined)
		}
	}
	check(false)
	if runtime.GOOS == "windows" {
		putFile(t, r, "win/asterbound_server.exe", "combined")
		check(true)
	}
	if err := r.Remove(path.Join(path.Dir(client), helperName())); err != nil {
		t.Fatal(err)
	}
	if root, _, _, err := installation(exe); err == nil {
		root.Close()
		t.Fatal("recognized a layout without a packaged updater")
	}
}

func TestTemporaryHelperCleanup(t *testing.T) {
	tmp := t.TempDir()
	for _, name := range []string{"TMPDIR", "TMP", "TEMP"} {
		t.Setenv(name, tmp)
	}
	// Windows can reserve a separate system temp directory for privileged
	// accounts. Never perform this test outside the owned temp root.
	expected, err := os.Stat(tmp)
	if err != nil {
		t.Fatal(err)
	}
	actual, err := os.Stat(os.TempDir())
	if err != nil || !os.SameFile(expected, actual) {
		t.Skip("OS does not honor per-process temp override")
	}
	token := strings.Repeat("a", 32)
	dir := filepath.Join(tmp, tempPrefix+token)
	if err := os.Mkdir(dir, 0700); err != nil {
		t.Fatal(err)
	}
	for _, file := range []string{helperName(), "unrelated"} {
		if err := os.WriteFile(filepath.Join(dir, file), []byte("keep"), 0600); err != nil {
			t.Fatal(err)
		}
	}
	if tempToken(filepath.Join(dir, helperName())) != token {
		t.Fatal("temp copy not recognized")
	}
	if err := removeTempHelper(token); err == nil {
		t.Fatal("did not report non-empty private directory")
	}
	if data, err := os.ReadFile(filepath.Join(dir, "unrelated")); err != nil || string(data) != "keep" {
		t.Fatal("deleted unrelated temp file")
	}
	if err := os.Remove(filepath.Join(dir, "unrelated")); err != nil {
		t.Fatal(err)
	}
	for range 2 {
		if err := removeTempHelper(token); err != nil {
			t.Fatal(err)
		}
	}
}

func TestParentLifetimeProcess(t *testing.T) {
	dir := os.Getenv("ASTERBOUND_TEST_PARENT_DIR")
	switch os.Getenv("ASTERBOUND_TEST_PROCESS_ROLE") {
	case "parent":
		exe, err := os.Executable()
		if err != nil {
			t.Fatal(err)
		}
		child := exec.Command(exe, "-test.run=^TestParentLifetimeProcess$")
		child.Env = append(os.Environ(), "ASTERBOUND_TEST_PROCESS_ROLE=helper")
		if err := child.Start(); err != nil {
			t.Fatal(err)
		}
		child.Process.Release()
		if _, err := io.Copy(io.Discard, os.Stdin); err != nil {
			t.Fatal(err)
		}
	case "helper":
		wait, closeHandle, err := watchParent(os.Getppid())
		if err != nil {
			t.Fatal(err)
		}
		defer closeHandle()
		if err := os.WriteFile(filepath.Join(dir, "watching"), nil, 0600); err != nil {
			t.Fatal(err)
		}
		if err := wait(); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(filepath.Join(dir, "exited"), nil, 0600); err != nil {
			t.Fatal(err)
		}
	}
}

func TestWaitsForActualParentExit(t *testing.T) {
	dir := t.TempDir()
	exe, err := os.Executable()
	if err != nil {
		t.Fatal(err)
	}
	parent := exec.Command(exe, "-test.run=^TestParentLifetimeProcess$")
	parent.Env = append(os.Environ(), "ASTERBOUND_TEST_PROCESS_ROLE=parent", "ASTERBOUND_TEST_PARENT_DIR="+dir)
	in, err := parent.StdinPipe()
	if err != nil {
		t.Fatal(err)
	}
	if err := parent.Start(); err != nil {
		t.Fatal(err)
	}
	defer in.Close()
	waitFor := func(file string) error {
		deadline := time.Now().Add(10 * time.Second)
		for time.Now().Before(deadline) {
			_, err := os.Stat(filepath.Join(dir, file))
			if !errors.Is(err, os.ErrNotExist) {
				return err
			}
			time.Sleep(15 * time.Millisecond)
		}
		return fmt.Errorf("timed out waiting for %s", file)
	}
	if err := waitFor("watching"); err != nil {
		t.Fatal(err)
	}
	if _, err := os.Stat(filepath.Join(dir, "exited")); !errors.Is(err, os.ErrNotExist) {
		t.Fatalf("reported exit with a live parent: %v", err)
	}
	in.Close()
	if err := parent.Wait(); err != nil {
		t.Fatal(err)
	}
	if err := waitFor("exited"); err != nil {
		t.Fatal(err)
	}
}
