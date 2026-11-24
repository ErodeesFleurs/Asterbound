// Command release packages a trusted assembled distribution. Its separate sign
// operation runs offline; only the public key ever belongs on a build runner.
package main

import (
	"archive/zip"
	"crypto/ed25519"
	"crypto/sha256"
	"crypto/x509"
	"encoding/hex"
	"encoding/json"
	"encoding/pem"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"maps"
	"os"
	"path/filepath"
	"slices"
	"time"

	"asterbound/updater/internal/release"
)

func main() {
	if err := run(os.Args[1:]); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func run(args []string) error {
	switch {
	case (len(args) == 6 || (len(args) == 7 && args[6] == "combined")) && args[0] == "pack":
		return pack(args[1], args[2], args[3], args[4], args[5], len(args) == 7)
	case len(args) == 3 && args[0] == "sign":
		der, err := readPEM(args[1], "PRIVATE KEY")
		if err != nil {
			return err
		}
		k, err := x509.ParsePKCS8PrivateKey(der)
		if err != nil {
			return err
		}
		key, ok := k.(ed25519.PrivateKey)
		if !ok {
			return errors.New("release key must be Ed25519")
		}
		f, err := os.Open(args[2])
		if err != nil {
			return err
		}
		defer f.Close()
		h := sha256.New()
		n, err := io.Copy(h, io.LimitReader(f, release.MaxArchive+1))
		if err != nil {
			return err
		}
		if n == 0 || n > release.MaxArchive {
			return errors.New("invalid release archive size")
		}
		sig := ed25519.Sign(key, h.Sum(nil))
		return newOutput(args[2]+".sig", func(out *os.File) error {
			_, err := out.Write(sig)
			return err
		})
	case len(args) == 2 && args[0] == "public-key":
		der, err := readPEM(args[1], "PUBLIC KEY")
		if err != nil {
			return err
		}
		k, err := x509.ParsePKIXPublicKey(der)
		if err != nil {
			return err
		}
		key, ok := k.(ed25519.PublicKey)
		if !ok {
			return errors.New("release key must be Ed25519")
		}
		fmt.Println(hex.EncodeToString(key))
		return nil
	}
	return errors.New("usage: release pack VERSION GOOS GOARCH ASSEMBLED_TREE OUTPUT_DIR [combined]\n       release sign PRIVATE_PKCS8_PEM ZIP\n       release public-key PUBLIC_PEM")
}

func readPEM(file, kind string) ([]byte, error) {
	data, err := os.ReadFile(file)
	if err != nil {
		return nil, err
	}
	block, rest := pem.Decode(data)
	if block == nil || block.Type != kind || len(rest) != 0 {
		return nil, fmt.Errorf("expected one %s PEM in %s", kind, file)
	}
	return block.Bytes, nil
}

// Never overwrite previously reviewed/signed release artifacts. Remove a
// partial artifact on any stream, flush or file close error.
func newOutput(name string, produce func(*os.File) error) error {
	out, err := os.OpenFile(name, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0644)
	if err != nil {
		return err
	}
	err = produce(out)
	closeErr := out.Close()
	if err == nil {
		err = closeErr
	}
	if err != nil {
		os.Remove(name)
	}
	return err
}

func pack(version, goos, arch, tree, output string, combined bool) error {
	if _, err := release.Version(version); err != nil {
		return err
	}
	files, err := release.Layout(goos, arch, combined)
	if err != nil {
		return err
	}
	root, err := os.OpenRoot(tree)
	if err != nil {
		return err
	}
	defer root.Close()
	manifest := release.Manifest{Version: version, OS: goos, Arch: arch, Files: slices.Sorted(maps.Keys(files))}
	return newOutput(filepath.Join(output, release.AssetName(goos, arch, combined)), func(out *os.File) error {
		z := zip.NewWriter(out)
		header := func(name string, mode fs.FileMode) (io.Writer, error) {
			h := &zip.FileHeader{Name: name, Method: zip.Deflate, Modified: time.Date(1980, 1, 1, 0, 0, 0, 0, time.UTC)}
			h.SetMode(mode)
			return z.CreateHeader(h)
		}
		w, err := header("update.json", 0644)
		if err != nil {
			return err
		}
		m, err := json.Marshal(manifest)
		if err != nil {
			return err
		}
		if _, err := w.Write(m); err != nil {
			return err
		}
		total := int64(len(m))
		for _, name := range manifest.Files {
			local := filepath.FromSlash(name)
			info, err := root.Lstat(local)
			if err != nil {
				return err
			}
			if !info.Mode().IsRegular() || info.Size() <= 0 || info.Size() > release.MaxMember {
				return fmt.Errorf("expected a regular, non-empty release file <= %d bytes: %s", release.MaxMember, name)
			}
			total += info.Size()
			if total > release.MaxExpanded {
				return errors.New("expanded release would exceed installer limit")
			}
			w, err := header(name, files[name])
			if err != nil {
				return err
			}
			f, err := root.Open(local)
			if err != nil {
				return err
			}
			n, err := io.Copy(w, io.LimitReader(f, info.Size()+1))
			f.Close()
			if err != nil {
				return err
			}
			if n != info.Size() {
				return fmt.Errorf("release file changed during packaging: %s", name)
			}
		}
		if err := z.Close(); err != nil {
			return err
		}
		s, err := out.Stat()
		if err != nil {
			return err
		}
		if s.Size() > release.MaxArchive {
			return errors.New("compressed release would exceed installer limit")
		}
		return nil
	})
}
