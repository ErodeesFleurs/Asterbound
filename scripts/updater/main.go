package main

import (
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"time"

	"asterbound/updater/internal/release"
)

const stageDir = ".asterbound-update"

func main() {
	if err := run(os.Args[1:]); err != nil {
		// The client's pipes and log may already be closed after COMMIT. Never
		// truncate an existing log or write through a user-controlled symlink.
		if f, e := os.CreateTemp("", "asterbound-update-error-*.log"); e == nil {
			fmt.Fprintf(f, "%s: %v\n", time.Now().Format(time.RFC3339), err)
			f.Close()
			fmt.Fprintf(os.Stderr, "Update diagnostic: %s\n", f.Name())
		}
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func run(args []string) (err error) {
	if runtime.GOOS == "windows" && len(args) == 2 && args[0] == "--cleanup-temp" {
		return removeTempHelper(args[1])
	}
	if len(args) < 4 || args[0] != "--install" {
		return errors.New("expected --install CURRENT TARGET ABSOLUTE-CLIENT [client arguments ...]")
	}
	self, err := os.Executable()
	if err != nil {
		return err
	}
	token := tempToken(self)
	if runtime.GOOS == "windows" && token == "" {
		return errors.New("start the updater from a private temporary copy using the client update button")
	}
	if runtime.GOOS != "windows" && token != "" {
		defer func() { err = errors.Join(err, removeTempHelper(token)) }()
	}
	waitParent, closeParent, err := watchParent(os.Getppid())
	if err != nil {
		return err
	}
	defer closeParent()
	root, client, combined, err := installation(args[3])
	if err != nil {
		return err
	}
	defer root.Close()

	stage := filepath.Join(root.Name(), stageDir)
	lock, err := claimStage(root)
	if err != nil {
		return err
	}
	keep := false
	if runtime.GOOS == "windows" {
		defer func() {
			if keep {
				err = errors.Join(err, fmt.Errorf("remove temporary helper directory %s%s manually", tempPrefix, token))
				return
			}
			installed := filepath.Join(root.Name(), filepath.FromSlash(path.Dir(client)), helperName())
			err = errors.Join(err, retireHelper(installed, token))
		}()
	}
	defer func() {
		if keep {
			return
		}
		now, e := root.Lstat(stageDir)
		if e == nil && !os.SameFile(now, lock) {
			e = fmt.Errorf("transaction directory was replaced: %s", stage)
		}
		if e == nil {
			e = root.RemoveAll(stageDir)
		}
		err = errors.Join(err, e)
	}()

	control, ctx, cancel := monitorClient(os.Stdin)
	defer cancel()
	files, err := prepare(ctx, stage, args[1], args[2], combined)
	if err != nil {
		return err
	}
	plan, err := preflight(root, files)
	if err != nil {
		return err
	}
	var oldClient, newClient os.FileInfo
	for _, p := range plan {
		if p.Path == client {
			oldClient, newClient = p.old, p.new
			break
		}
	}
	if oldClient == nil || newClient == nil {
		return errors.New("missing installed or staged client")
	}
	if err := control.requestCommit(ctx, os.Stdout); err != nil {
		return err
	}
	if err := waitParent(); err != nil {
		return fmt.Errorf("client did not exit; installation unchanged: %w", err)
	}
	launch := func(image os.FileInfo) error {
		if err := unchanged(root, client, image); err != nil {
			return err
		}
		return startClient(filepath.Join(root.Name(), filepath.FromSlash(client)), args[4:])
	}
	keep, err = apply(root, plan, func() error { return launch(newClient) })
	if keep {
		return fmt.Errorf("rollback incomplete; do not launch. Recover originals from %s/old using plan.json: %w", stage, err)
	}
	if err != nil {
		if restartErr := launch(oldClient); restartErr != nil {
			return fmt.Errorf("installation rolled back; restart the previous client manually: %w", errors.Join(err, restartErr))
		}
		return fmt.Errorf("installation rolled back; restarted the previous client: %w", err)
	}
	return nil
}

func helperName() string {
	if runtime.GOOS == "windows" {
		return "asterbound-updater.exe"
	}
	return "asterbound-updater"
}

// Use the packaged client's exact suffix, not the cwd, archive filename, or
// sbinit contents. sbinit is user-editable and is never an update destination.
func installation(exe string) (*os.Root, string, bool, error) {
	exe = filepath.FromSlash(exe)
	client := release.ClientPath(runtime.GOOS)
	if client == "" || !filepath.IsAbs(exe) || filepath.Clean(exe) != exe {
		return nil, "", false, errors.New("client must be a clean absolute standalone executable path")
	}
	base := exe
	for range strings.Count(client, "/") + 1 {
		base = filepath.Dir(base)
	}
	if filepath.Join(base, filepath.FromSlash(client)) != exe {
		return nil, "", false, fmt.Errorf("not a standalone client: expected %s", client)
	}
	fi, err := os.Lstat(base)
	if err != nil {
		return nil, "", false, err
	}
	if !fi.IsDir() || isLink(fi) {
		return nil, "", false, fmt.Errorf("unsafe installation directory: %s", base)
	}
	base, err = filepath.EvalSymlinks(base)
	if err != nil {
		return nil, "", false, err
	}
	root, err := os.OpenRoot(base)
	if err != nil {
		return nil, "", false, err
	}
	platform, _, _ := strings.Cut(client, "/")
	for _, marker := range []string{client, path.Join(path.Dir(client), helperName()), path.Join(platform, "sbinit.config"), "assets/asterbound.pak"} {
		if _, err := regularFile(root, marker, false); err != nil {
			root.Close()
			return nil, "", false, fmt.Errorf("not a complete standalone installation: %w", err)
		}
	}
	combined := false
	if runtime.GOOS == "windows" {
		server, err := regularFile(root, "win/asterbound_server.exe", true)
		if err != nil {
			root.Close()
			return nil, "", false, err
		}
		combined = server != nil
	}
	return root, client, combined, nil
}

func claimStage(root *os.Root) (os.FileInfo, error) {
	// The staging directory IS the exclusive installation lock. Even a crash
	// mid-rename cannot allow a later updater to overwrite the only backup.
	if err := root.Mkdir(stageDir, 0700); err != nil {
		if errors.Is(err, os.ErrExist) {
			return nil, fmt.Errorf("update already active or interrupted at %s: stop other updaters first; preserve this directory, inspect plan.json and old/ for originals before restoring files and removing the directory", filepath.Join(root.Name(), stageDir))
		}
		return nil, fmt.Errorf("installation must be writable without elevation: %w", err)
	}
	info, err := root.Lstat(stageDir)
	if err != nil {
		return nil, err
	}
	if !info.IsDir() || isLink(info) {
		return nil, errors.New("invalid transaction directory")
	}
	return info, nil
}

type updateControl struct {
	mu        sync.Mutex
	ready     bool
	committed chan struct{}
}

// Watch the pipe from the beginning of preparation. UI cancellation or
// parent death must interrupt an in-flight HTTPS request rather than hold the
// update lock until a slow transfer finishes. No unbounded command buffering.
func monitorClient(input io.Reader) (*updateControl, context.Context, context.CancelFunc) {
	c := &updateControl{committed: make(chan struct{})}
	ctx, cancel := context.WithCancelCause(context.Background())
	go func() {
		var command [len("COMMIT\n")]byte
		_, err := io.ReadFull(input, command[:])
		c.mu.Lock()
		defer c.mu.Unlock()
		switch {
		case err != nil || string(command[:]) != "COMMIT\n":
			cancel(errors.New("update cancelled: client did not send COMMIT"))
		case !c.ready:
			cancel(errors.New("unexpected COMMIT before READY"))
		default:
			close(c.committed)
		}
	}()
	return c, ctx, func() { cancel(context.Canceled) }
}

func (c *updateControl) requestCommit(ctx context.Context, output io.Writer) error {
	restore := guardBrokenPipe()
	defer restore()
	c.mu.Lock()
	err := context.Cause(ctx)
	if err == nil {
		_, err = io.WriteString(output, "READY\n")
		c.ready = err == nil
	}
	c.mu.Unlock()
	if err != nil {
		return err
	}
	timer := time.NewTimer(2 * time.Minute)
	defer timer.Stop()
	select {
	case <-c.committed:
		return nil
	case <-ctx.Done():
		return context.Cause(ctx)
	case <-timer.C:
		return errors.New("timed out waiting for client COMMIT; installation unchanged")
	}
}

func startClient(exe string, args []string) error {
	// No shell, PATH lookup, updater pipes, or inferred working directory.
	cmd := exec.Command(exe, args...)
	if err := cmd.Start(); err != nil {
		return fmt.Errorf("launch %s: %w", exe, err)
	}
	// Process creation succeeded; a handle-cleanup error cannot make it safe
	// to roll back files underneath the running game.
	_ = cmd.Process.Release()
	return nil
}

const tempPrefix = "asterbound-update-"

func validToken(s string) bool {
	return len(s) == 32 && strings.Trim(s, "0123456789abcdef") == ""
}

func tempToken(exe string) string {
	dir := filepath.Dir(exe)
	token, ok := strings.CutPrefix(filepath.Base(dir), tempPrefix)
	if !ok || !validToken(token) || filepath.Base(exe) != helperName() {
		return ""
	}
	// os.Executable and os.TempDir can disagree about canonical spellings
	// (notably /var vs /private/var on macOS).
	parent, err := os.Stat(filepath.Dir(dir))
	tmp, tmpErr := os.Stat(os.TempDir())
	if err != nil || tmpErr != nil || !os.SameFile(parent, tmp) {
		return ""
	}
	return token
}

// Delete only the fixed executable and its now-empty private directory. Never
// recursively remove files from a shared temporary location.
func removeTempHelper(token string) error {
	if !validToken(token) {
		return errors.New("invalid temporary helper directory")
	}
	r, err := os.OpenRoot(os.TempDir())
	if err != nil {
		return err
	}
	defer r.Close()
	dir := tempPrefix + token
	file := path.Join(dir, helperName())
	original, err := regularFile(r, file, true)
	if errors.Is(err, os.ErrNotExist) {
		return nil // The UI already removed its private copy and directory.
	}
	if err != nil {
		return err
	}
	deadline := time.Now().Add(15 * time.Second)
	for original != nil {
		if err := unchanged(r, file, original); err != nil {
			if _, e := r.Lstat(file); errors.Is(e, os.ErrNotExist) {
				break
			}
			return err
		}
		err = r.Remove(file)
		if err == nil || errors.Is(err, os.ErrNotExist) {
			break
		}
		if runtime.GOOS != "windows" || time.Now().After(deadline) {
			return fmt.Errorf("remove temporary helper manually: %s: %w", filepath.Join(r.Name(), file), err)
		}
		time.Sleep(100 * time.Millisecond)
	}
	err = r.Remove(dir)
	if errors.Is(err, os.ErrNotExist) {
		return nil
	}
	return err
}
