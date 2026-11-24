package main

import (
	"archive/zip"
	"bytes"
	"context"
	"crypto/ed25519"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"io/fs"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"time"

	"asterbound/updater/internal/release"
)

// Set only when building an official standalone helper (-ldflags -X).
var publicKeyHex string

type payloadPlan struct {
	target string
	files  map[string]fs.FileMode
	key    ed25519.PublicKey
}

func newPayloadPlan(current, target string, combined bool) (payloadPlan, error) {
	var p payloadPlan
	from, err := release.Version(current)
	if err != nil {
		return p, err
	}
	to, err := release.Version(target)
	if err != nil {
		return p, err
	}
	if to <= from {
		return p, errors.New("target must be newer than the installed version")
	}
	files, err := release.Layout(runtime.GOOS, runtime.GOARCH, combined)
	if err != nil {
		return p, err
	}
	key, err := hex.DecodeString(publicKeyHex)
	if err != nil || len(key) != ed25519.PublicKeySize {
		return p, errors.New("this helper has no valid pinned release key")
	}
	return payloadPlan{target, files, key}, nil
}

func prepare(ctx context.Context, stage, current, target string, combined bool) ([]string, error) {
	p, err := newPayloadPlan(current, target, combined)
	if err != nil {
		return nil, err
	}
	ctx, cancel := context.WithTimeout(ctx, 15*time.Minute)
	defer cancel()
	client := &http.Client{
		CheckRedirect: func(req *http.Request, via []*http.Request) error {
			u := req.URL
			if len(via) > 4 || u.Scheme != "https" || u.User != nil || (u.Port() != "" && u.Port() != "443") {
				return errors.New("invalid release redirect")
			}
			switch u.Hostname() {
			case "github.com", "release-assets.githubusercontent.com", "objects.githubusercontent.com":
				return nil
			}
			return errors.New("unexpected release download host")
		},
	}
	defer client.CloseIdleConnections()
	url := "https://github.com/ErodeesFleurs/Asterbound/releases/download/v" + target + "/" + release.AssetName(runtime.GOOS, runtime.GOARCH, combined)
	get := func(url string, limit int64) (io.ReadCloser, error) {
		req, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
		if err != nil {
			return nil, err
		}
		res, err := client.Do(req)
		if err != nil {
			return nil, err
		}
		if res.StatusCode != http.StatusOK || res.ContentLength > limit {
			res.Body.Close()
			return nil, fmt.Errorf("release download refused (HTTP %d, length %d)", res.StatusCode, res.ContentLength)
		}
		return res.Body, nil
	}
	sigBody, err := get(url+".sig", ed25519.SignatureSize)
	if err != nil {
		return nil, err
	}
	sig, err := io.ReadAll(io.LimitReader(sigBody, ed25519.SignatureSize+1))
	sigBody.Close()
	if err != nil || len(sig) != ed25519.SignatureSize {
		return nil, errors.New("release signature is missing or malformed")
	}
	body, err := get(url, release.MaxArchive)
	if err != nil {
		return nil, err
	}
	defer body.Close()
	return p.stage(ctx, stage, body, sig)
}

// For isolated signed-archive verification. This is not a runtime flag, and
// uses the very same key, platform, version and extraction checks as HTTPS.
func prepareSigned(stage, current, target string, combined bool, archive io.Reader, signature []byte) ([]string, error) {
	p, err := newPayloadPlan(current, target, combined)
	if err != nil {
		return nil, err
	}
	return p.stage(context.Background(), stage, archive, signature)
}

func (p payloadPlan) stage(ctx context.Context, stage string, archive io.Reader, sig []byte) ([]string, error) {
	if len(sig) != ed25519.SignatureSize {
		return nil, errors.New("invalid release signature size")
	}
	info, err := os.Lstat(stage)
	if err != nil {
		return nil, err
	}
	if !info.IsDir() || isLink(info) {
		return nil, errors.New("invalid transaction directory")
	}
	workspace, err := os.OpenRoot(stage)
	if err != nil {
		return nil, err
	}
	defer workspace.Close()
	pinned, err := workspace.Stat(".")
	if err != nil || !os.SameFile(pinned, info) {
		return nil, errors.New("transaction directory changed before extraction")
	}
	f, err := workspace.OpenFile("update.zip", os.O_RDWR|os.O_CREATE|os.O_EXCL, 0600)
	if err != nil {
		return nil, err
	}
	defer workspace.Remove("update.zip")
	defer f.Close()
	h := sha256.New()
	n, err := io.Copy(io.MultiWriter(f, h), io.LimitReader(archive, release.MaxArchive+1))
	if err != nil {
		return nil, err
	}
	if n > release.MaxArchive || !ed25519.Verify(p.key, h.Sum(nil), sig) {
		return nil, errors.New("release ZIP is oversized or its signature is invalid")
	}

	// Our release format uses plain, comment-free ZIP32. Bound the central
	// directory before archive/zip allocates names, comments and extra fields.
	var end [22]byte
	if n < int64(len(end)) {
		return nil, zip.ErrFormat
	}
	if _, err := f.ReadAt(end[:], n-int64(len(end))); err != nil {
		return nil, err
	}
	le := binary.LittleEndian
	count := uint16(len(p.files) + 1)
	dirSize := int64(le.Uint32(end[12:]))
	if string(end[:4]) != "PK\x05\x06" || le.Uint32(end[4:]) != 0 || le.Uint16(end[8:]) != count || le.Uint16(end[10:]) != count ||
		le.Uint16(end[20:]) != 0 || dirSize > 64<<10 || int64(le.Uint32(end[16:]))+dirSize+22 != n {
		return nil, errors.New("invalid release ZIP directory")
	}
	z, err := zip.NewReader(f, n)
	if err != nil {
		return nil, err
	}
	if len(z.File) != int(count) {
		return nil, errors.New("unexpected number of ZIP entries")
	}
	entries := make(map[string]*zip.File, len(z.File))
	var total uint64
	for _, entry := range z.File {
		_, allowed := p.files[entry.Name]
		if (!allowed && entry.Name != "update.json") || entries[entry.Name] != nil || !entry.Mode().IsRegular() || entry.Flags&1 != 0 ||
			(entry.Method != zip.Store && entry.Method != zip.Deflate) {
			return nil, fmt.Errorf("unexpected, duplicate or non-regular ZIP entry %q", entry.Name)
		}
		size := entry.UncompressedSize64
		if size == 0 || size > release.MaxMember || (entry.Name == "update.json" && size > release.MaxManifest) {
			return nil, fmt.Errorf("invalid size for %q", entry.Name)
		}
		total += size
		if total > release.MaxExpanded {
			return nil, errors.New("expanded update exceeds limit")
		}
		entries[entry.Name] = entry
	}
	// Exact entry count + unique allowlisted names imply update.json exists.
	var raw bytes.Buffer
	if err := copyEntry(ctx, &raw, entries["update.json"]); err != nil {
		return nil, err
	}
	m, err := decodeManifest(raw.Bytes())
	if err != nil {
		return nil, err
	}
	if m.Version != p.target || m.OS != runtime.GOOS || m.Arch != runtime.GOARCH || len(m.Files) != len(p.files) {
		return nil, errors.New("manifest version, platform or file set does not match the requested update")
	}
	seen := make(map[string]bool, len(m.Files))
	for _, name := range m.Files {
		if _, ok := p.files[name]; !ok || seen[name] {
			return nil, fmt.Errorf("unexpected or duplicate manifest path %q", name)
		}
		seen[name] = true
	}

	if err := workspace.Mkdir("new", 0700); err != nil {
		return nil, err
	}
	root, err := workspace.OpenRoot("new")
	if err != nil {
		return nil, err
	}
	defer root.Close()
	for _, name := range m.Files {
		local := filepath.FromSlash(name)
		if err := root.MkdirAll(filepath.Dir(local), 0755); err != nil {
			return nil, err
		}
		out, err := root.OpenFile(local, os.O_WRONLY|os.O_CREATE|os.O_EXCL, p.files[name])
		if err != nil {
			return nil, err
		}
		err = copyEntry(ctx, out, entries[name])
		closeErr := out.Close()
		if err != nil {
			return nil, err
		}
		if closeErr != nil {
			return nil, closeErr
		}
	}
	return m.Files, nil
}

// Deflate can spend substantial time on a single member after the network
// transfer is complete. Stop that work too when the client closes its pipe.
type cancellableEntry struct {
	ctx context.Context
	r   io.Reader
}

func (c cancellableEntry) Read(b []byte) (int, error) {
	if err := c.ctx.Err(); err != nil {
		return 0, err
	}
	return c.r.Read(b)
}

func copyEntry(ctx context.Context, dst io.Writer, entry *zip.File) error {
	r, err := entry.Open()
	if err != nil {
		return err
	}
	defer r.Close()
	size := int64(entry.UncompressedSize64)
	n, err := io.Copy(dst, io.LimitReader(cancellableEntry{ctx, r}, size+1)) // read to EOF for archive/zip's CRC check
	if err != nil {
		return err
	}
	if n != size {
		return fmt.Errorf("wrong expanded size for %q", entry.Name)
	}
	return nil
}

// encoding/json's default object handling accepts duplicate and case-folded
// keys. A signed manifest has exactly one occurrence of each documented field.
func decodeManifest(raw []byte) (release.Manifest, error) {
	var m release.Manifest
	d := json.NewDecoder(bytes.NewReader(raw))
	if token, err := d.Token(); err != nil || token != json.Delim('{') {
		return m, errors.New("expected manifest object")
	}
	fields := map[string]any{"version": &m.Version, "os": &m.OS, "arch": &m.Arch, "files": &m.Files}
	for d.More() {
		k, err := d.Token()
		if err != nil {
			return m, err
		}
		key, ok := k.(string)
		dst := fields[key]
		if !ok || dst == nil {
			return m, errors.New("unknown or duplicate manifest field")
		}
		if err := d.Decode(dst); err != nil {
			return m, err
		}
		delete(fields, key)
	}
	if token, err := d.Token(); err != nil || token != json.Delim('}') || len(fields) != 0 {
		return m, errors.New("incomplete manifest")
	}
	if _, err := d.Token(); err != io.EOF {
		return m, errors.New("trailing manifest content")
	}
	return m, nil
}
