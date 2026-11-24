package main

import (
	"errors"
	"fmt"
	"os"
	"os/exec"
	"syscall"
)

func watchParent(pid int) (wait func() error, close func(), err error) {
	if pid <= 0 {
		return nil, nil, errors.New("missing client parent")
	}
	// Open while the parent is still waiting for READY. Waiting on the handle
	// (not the numeric PID) also waits for the original image to be unmapped.
	h, err := syscall.OpenProcess(syscall.SYNCHRONIZE, false, uint32(pid))
	if err != nil {
		return nil, nil, fmt.Errorf("open client process: %w", err)
	}
	return func() error {
		status, err := syscall.WaitForSingleObject(h, 120000)
		if err != nil {
			return err
		}
		if status != syscall.WAIT_OBJECT_0 {
			return fmt.Errorf("waiting for client exit returned status %d", status)
		}
		return nil
	}, func() { syscall.CloseHandle(h) }, nil
}

func guardBrokenPipe() func() { return func() {} }

func isLink(info os.FileInfo) bool {
	attrs, ok := info.Sys().(*syscall.Win32FileAttributeData)
	return !ok || attrs.FileAttributes&syscall.FILE_ATTRIBUTE_REPARSE_POINT != 0
}

func retireHelper(installed, token string) error {
	// A mapped PE cannot unlink itself. Only AFTER replacement/rollback,
	// run the installed sibling; it accepts a token, never an arbitrary path
	// or shell command, and waits for our image lock to disappear.
	cmd := exec.Command(installed, "--cleanup-temp", token)
	cmd.SysProcAttr = &syscall.SysProcAttr{HideWindow: true}
	if err := cmd.Start(); err != nil {
		return fmt.Errorf("remove temporary helper directory %s%s manually: %w", tempPrefix, token, err)
	}
	_ = cmd.Process.Release()
	return nil
}

func retryRename(err error) bool {
	// File scanners sometimes briefly hold another handle after client exit.
	return errors.Is(err, syscall.Errno(32)) || errors.Is(err, syscall.Errno(33))
}
