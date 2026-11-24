// Package release defines the update format shared by the installer and the
// offline release packer. Never expand a glob in the installation directory:
// additions to the set of vendor-owned files require an explicit decision.
package release

import (
	"fmt"
	"io/fs"
	"path"
	"strconv"
	"strings"
)

const (
	MaxArchive = 1 << 30
	MaxMember = 512 << 20
	MaxExpanded = 2 << 30
	MaxManifest = 16 << 10
)

type Manifest struct {
	Version string   `json:"version"`
	OS      string   `json:"os"`
	Arch    string   `json:"arch"`
	Files   []string `json:"files"`
}

// Version maps exactly four canonical decimal uint16 components to a number
// with the same ordering (cmake/AsterboundVersion.cmake).
func Version(s string) (uint64, error) {
	var result uint64
	parts := strings.Split(s, ".")
	if len(parts) == 4 {
		for _, part := range parts {
			n, err := strconv.ParseUint(part, 10, 16)
			if err != nil || strconv.FormatUint(n, 10) != part {
				return 0, fmt.Errorf("invalid product version %q", s)
			}
			result = result<<16 | n
		}
		return result, nil
	}
	return 0, fmt.Errorf("invalid product version %q", s)
}

func AssetName(goos, arch string, combined bool) string {
	suffix := ""
	if combined {
		suffix = "-combined"
	}
	return "Asterbound-update-" + goos + "-" + arch + suffix + ".zip"
}

func ClientPath(goos string) string {
	switch goos {
	case "linux":
		return "linux/asterbound"
	case "windows":
		return "win/asterbound.exe"
	case "darwin":
		return "osx/Asterbound.app/Contents/MacOS/asterbound"
	}
	return ""
}

// Layout is deliberately exhaustive, not a list of extension or directory
// patterns. In particular it cannot select user mods, packed.pak, sbinit.config,
// storage, logs, debug symbols or the test-fixture DLL.
func Layout(goos, arch string, combined bool) (map[string]fs.FileMode, error) {
	client := ClientPath(goos)
	if client == "" || (arch != "amd64" && arch != "arm64") || (goos == "windows" && arch != "amd64") || (combined && goos != "windows") {
		return nil, fmt.Errorf("unsupported update layout: %s/%s (combined=%t)", goos, arch, combined)
	}
	files := map[string]fs.FileMode{
		"assets/asterbound.pak":        0644,
		"assets/asterbound-compat.pak": 0644,
		"assets/asterbound-legacy.pak": 0644,
		client:                         0755,
	}
	bindir := path.Dir(client)
	ext := ""
	if goos == "windows" {
		ext = ".exe"
	}
	files[bindir+"/asterbound-updater"+ext] = 0755
	tools := bindir
	switch goos {
	case "linux":
		files["linux/.icon/asterbound.png"] = 0644
		files["linux/libsteam_api.so"] = 0644
		files["linux/libdiscord_game_sdk.so"] = 0644
		files["linux/run-client.sh"] = 0755
	case "windows":
		for _, dll := range []string{"SDL3.dll", "vulkan-1.dll", "steam_api64.dll", "discord_game_sdk.dll"} {
			files["win/"+dll] = 0644
		}
		if combined {
			files["win/asterbound_server.exe"] = 0755
		}
	case "darwin":
		tools = "osx"
		files["osx/asterbound_server"] = 0755
		files["osx/run-server.sh"] = 0755
		files[bindir+"/libsteam_api.dylib"] = 0644
		if arch == "arm64" {
			files[bindir+"/discord_game_sdk.dylib"] = 0644
		}
		files["osx/Asterbound.app/Contents/Info.plist"] = 0644
		files["osx/Asterbound.app/Contents/Resources/asterbound.icns"] = 0644
	}
	for _, tool := range []string{"asset_packer", "asset_unpacker", "btree_repacker", "dump_versioned_json", "make_versioned_json"} {
		files[tools+"/"+tool+ext] = 0755
	}
	files[tools+"/OPENSOURCE.md"] = 0644
	files[tools+"/steam_appid.txt"] = 0644
	return files, nil
}
