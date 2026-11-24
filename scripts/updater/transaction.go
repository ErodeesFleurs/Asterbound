package main

import (
	"crypto/rand"
	"encoding/json"
	"errors"
	"fmt"
	"io/fs"
	"os"
	"path"
	"strings"
	"time"
)

// regularFile rejects links (including Windows non-symlink reparse points) in
// every component, even when they would resolve somewhere else *inside* the
// root, e.g. mods/. Mutations additionally use os.Root, not joined absolute
// paths: an exchanged symlink cannot redirect a rename outside the install.
func regularFile(root *os.Root, name string, optional bool) (os.FileInfo, error) {
	if !fs.ValidPath(name) || name == "." || strings.ContainsAny(name, `\:`) {
		return nil, fmt.Errorf("invalid relative filename %q", name)
	}
	for i := range len(name) {
		if name[i] != '/' {
			continue
		}
		dir, err := root.Lstat(name[:i])
		if err != nil {
			return nil, err
		}
		if !dir.IsDir() || isLink(dir) {
			return nil, fmt.Errorf("refusing non-directory or link: %s", name[:i])
		}
	}
	info, err := root.Lstat(name)
	if optional && errors.Is(err, os.ErrNotExist) {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	if !info.Mode().IsRegular() || isLink(info) {
		return nil, fmt.Errorf("refusing non-regular file or link: %s", name)
	}
	return info, nil
}

func unchanged(root *os.Root, name string, want os.FileInfo) error {
	have, err := regularFile(root, name, true)
	if err != nil {
		return err
	}
	if have == nil && want == nil {
		return nil
	}
	if have != nil && want != nil && os.SameFile(have, want) && have.Size() == want.Size() && have.Mode() == want.Mode() && have.ModTime().Equal(want.ModTime()) {
		return nil
	}
	return fmt.Errorf("file changed during update: %s", name)
}

type replacement struct {
	Path     string `json:"path"`
	Existing bool   `json:"existing"`
	old, new os.FileInfo
	backedUp bool
	placed   bool
}

func staged(name string) string { return path.Join(stageDir, "new", name) }
func backup(name string) string { return path.Join(stageDir, "old", name) }

// files is the exact platform allowlist already verified in prepare. No
// directory enumeration, deletions inferred from the archive, or mutable user
// paths are part of a transaction. Missing directories fail before READY.
func preflight(root *os.Root, files []string) ([]replacement, error) {
	plan := make([]replacement, 0, len(files))
	dirs := make(map[string]bool)
	for _, file := range files {
		old, err := regularFile(root, file, true)
		if err != nil {
			return nil, err
		}
		newFile, err := regularFile(root, staged(file), false)
		if err != nil {
			return nil, err
		}
		dir := path.Dir(file)
		if !dirs[dir] {
			if err := root.MkdirAll(path.Dir(backup(file)), 0700); err != nil {
				return nil, err
			}
			if err := probeWritable(root, dir); err != nil {
				return nil, fmt.Errorf("cannot stage a rename for %s (need writable directories on one filesystem): %w", dir, err)
			}
			dirs[dir] = true
		}
		plan = append(plan, replacement{Path: file, Existing: old != nil, old: old, new: newFile})
	}
	// Stale state is never automatically replayed or deleted. Preserve the
	// original presence/absence of each destination in case we lose power
	// between renames. old/ mirrors the exact installation paths.
	f, err := root.OpenFile(path.Join(stageDir, "plan.json"), os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if err != nil {
		return nil, err
	}
	err = json.NewEncoder(f).Encode(plan)
	if err == nil {
		err = f.Sync()
	}
	return plan, errors.Join(err, f.Close())
}

func probeWritable(root *os.Root, dir string) error {
	probe := path.Join(dir, ".asterbound-write-test-"+rand.Text())
	f, err := root.OpenFile(probe, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0600)
	if err != nil {
		return err
	}
	if err := f.Close(); err != nil {
		return errors.Join(err, root.Remove(probe))
	}
	inside := path.Join(stageDir, "write-test")
	if err := root.Rename(probe, inside); err != nil {
		return errors.Join(err, root.Remove(probe))
	}
	return root.Remove(inside)
}

// rename retries only short Windows sharing/lock violations. Never change file
// permissions or terminate another program that holds an installation file.
func rename(root *os.Root, from, to string) error {
	deadline := time.Now().Add(3 * time.Second)
	for {
		err := root.Rename(from, to)
		if err == nil || !retryRename(err) || time.Now().After(deadline) {
			return err
		}
		time.Sleep(100 * time.Millisecond)
	}
}

// The game has exited. Keep originals until the new CreateProcess/exec has
// succeeded. If rollback itself fails, the caller MUST retain the stage.
func apply(root *os.Root, plan []replacement, launch func() error) (keepStage bool, err error) {
	defer func() {
		if err != nil {
			if restoreErr := rollback(root, plan); restoreErr != nil {
				keepStage = true
				err = errors.Join(err, restoreErr)
			}
		}
	}()
	for i := range plan {
		p := &plan[i]
		if err := unchanged(root, p.Path, p.old); err != nil {
			return false, err
		}
		if err := unchanged(root, staged(p.Path), p.new); err != nil {
			return false, err
		}
		if p.Existing {
			if err := rename(root, p.Path, backup(p.Path)); err != nil {
				return false, err
			}
			p.backedUp = true
		}
		if err := rename(root, staged(p.Path), p.Path); err != nil {
			return false, err
		}
		p.placed = true
	}
	return false, launch()
}

func rollback(root *os.Root, plan []replacement) error {
	var failures error
	for i := len(plan) - 1; i >= 0; i-- {
		p := &plan[i]
		if !p.placed && !p.backedUp {
			continue
		}
		err := func() error {
			if p.placed {
				if err := unchanged(root, p.Path, p.new); err != nil {
					return err
				}
				if err := rename(root, p.Path, staged(p.Path)); err != nil {
					return err
				}
			}
			if p.backedUp {
				if err := unchanged(root, p.Path, nil); err != nil {
					return err
				}
				if err := unchanged(root, backup(p.Path), p.old); err != nil {
					return err
				}
				return rename(root, backup(p.Path), p.Path)
			}
			return nil
		}()
		failures = errors.Join(failures, err)
	}
	return failures
}
