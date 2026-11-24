//go:build linux || darwin

package main

import (
	"errors"
	"os"
	"os/signal"
	"syscall"
	"time"
)

func watchParent(pid int) (wait func() error, close func(), err error) {
	if pid <= 1 {
		return nil, nil, errors.New("missing client parent")
	}
	return func() error {
		deadline := time.Now().Add(2 * time.Minute)
		// Reparenting, unlike polling kill(pid, 0), cannot confuse a reused
		// PID or an unreaped zombie with the original, still-running client.
		for os.Getppid() == pid {
			if time.Now().After(deadline) {
				return errors.New("timed out waiting for client exit")
			}
			time.Sleep(50 * time.Millisecond)
		}
		return nil
	}, func() {}, nil
}

func guardBrokenPipe() func() {
	// A closed stdout must return EPIPE (so the staging lock is removed),
	// rather than terminate the helper with the default SIGPIPE action.
	signals := make(chan os.Signal, 1)
	signal.Notify(signals, syscall.SIGPIPE)
	return func() { signal.Stop(signals) }
}

func isLink(info os.FileInfo) bool { return info.Mode()&os.ModeSymlink != 0 }

func retireHelper(_ string, token string) error { return removeTempHelper(token) }

func retryRename(error) bool { return false }
