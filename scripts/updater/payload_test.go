package main

import (
	"archive/zip"
	"bytes"
	"crypto/ed25519"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"io/fs"
	"maps"
	"os"
	"path/filepath"
	"runtime"
	"slices"
	"testing"

	"asterbound/updater/internal/release"
)

func TestSignedPayload(t *testing.T) {
	files, err := release.Layout(runtime.GOOS, runtime.GOARCH, false)
	if err != nil {
		t.Skip(err)
	}
	key, secret, err := ed25519.GenerateKey(rand.Reader)
	if err != nil {
		t.Fatal(err)
	}
	oldKey := publicKeyHex
	t.Cleanup(func() { publicKeyHex = oldKey })
	publicKeyHex = hex.EncodeToString(key)
	const current, target = "0.9.10.0", "0.10.0.0"
	names := slices.Sorted(maps.Keys(files))
	for _, scenario := range []string{"valid", "corrupt", "wrong key", "replay", "version", "platform", "arch", "missing", "manifest duplicate", "field duplicate", "archive duplicate", "traversal", "user file", "symlink", "directory"} {
		t.Run(scenario, func(t *testing.T) {
			m := release.Manifest{Version: target, OS: runtime.GOOS, Arch: runtime.GOARCH, Files: slices.Clone(names)}
			switch scenario {
			case "version":
				m.Version = current
			case "platform":
				m.OS = "other"
			case "arch":
				m.Arch = "other"
			case "manifest duplicate":
				m.Files[0] = m.Files[1]
			}
			raw, err := json.Marshal(m)
			if err != nil {
				t.Fatal(err)
			}
			if scenario == "field duplicate" {
				raw = append([]byte(`{"version":"0.0.0.0",`), raw[1:]...)
			}
			var b bytes.Buffer
			z := zip.NewWriter(&b)
			write := func(name string, mode fs.FileMode, body []byte) {
				h := &zip.FileHeader{Name: name, Method: zip.Store}
				h.SetMode(mode)
				w, err := z.CreateHeader(h)
				if err != nil {
					t.Fatal(err)
				}
				if _, err := w.Write(body); err != nil {
					t.Fatal(err)
				}
			}
			write("update.json", 0644, raw)
			for i, name := range names {
				mode := files[name]
				if i == 0 {
					switch scenario {
					case "missing":
						continue
					case "archive duplicate":
						name = names[1]
					case "traversal":
						name = "../asterbound"
					case "user file":
						name = "assets/packed.pak"
					case "symlink":
						mode = fs.ModeSymlink | 0777
					case "directory":
						mode = fs.ModeDir | 0755
					}
				}
				write(name, mode, []byte(name))
			}
			if err := z.Close(); err != nil {
				t.Fatal(err)
			}
			digest := sha256.Sum256(b.Bytes())
			sig := ed25519.Sign(secret, digest[:])
			from := current
			switch scenario {
			case "corrupt":
				b.Bytes()[0] ^= 1
			case "wrong key":
				_, wrong, err := ed25519.GenerateKey(rand.Reader)
				if err != nil {
					t.Fatal(err)
				}
				sig = ed25519.Sign(wrong, digest[:])
			case "replay":
				from = target
			}
			stage := t.TempDir()
			got, err := prepareSigned(stage, from, target, false, &b, sig)
			if scenario != "valid" {
				if err == nil {
					t.Fatal("invalid update was accepted")
				}
				if _, err := os.Stat(filepath.Join(stage, "new")); !os.IsNotExist(err) {
					t.Fatal("unexpected staged payload", err)
				}
				return
			}
			if err != nil || !slices.Equal(got, names) {
				t.Fatal(got, err)
			}
			for _, name := range got {
				data, err := os.ReadFile(filepath.Join(stage, "new", filepath.FromSlash(name)))
				if err != nil || string(data) != name {
					t.Fatal(name, err)
				}
			}
		})
	}
}
