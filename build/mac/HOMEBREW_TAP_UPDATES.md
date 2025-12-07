# Homebrew Tap Update Recommendations

This document outlines recommended updates to the `homebrew-performancecopilot` tap to align with the improvements made to the macOS .pkg uninstaller.

## Repository Location
https://github.com/performancecopilot/homebrew-performancecopilot

## Files to Update

### 1. Casks/pcp-perf.rb

The cask formula should be updated to ensure complete cleanup during uninstall and provide a `zap` option for deep cleanup.

#### Current uninstall block (verify this is present):
```ruby
uninstall launchctl: [
            "io.pcp.pmcd",
            "io.pcp.pmie",
            "io.pcp.pmlogger",
            "io.pcp.pmproxy",
          ],
          pkgutil:   "io.pcp.performancecopilot"
```

#### Recommended enhancement - add explicit plist deletion:
```ruby
uninstall launchctl: [
            "io.pcp.pmcd",
            "io.pcp.pmie",
            "io.pcp.pmlogger",
            "io.pcp.pmproxy",
          ],
          pkgutil:   "io.pcp.performancecopilot",
          delete:    [
            "/Library/LaunchDaemons/io.pcp.pmcd.plist",
            "/Library/LaunchDaemons/io.pcp.pmie.plist",
            "/Library/LaunchDaemons/io.pcp.pmlogger.plist",
            "/Library/LaunchDaemons/io.pcp.pmproxy.plist",
          ]
```

#### Add new zap stanza for deep cleanup:
```ruby
zap trash: [
      "/etc/pcp",
      "/var/lib/pcp",
      "/var/log/pcp",
    ]
```

This allows users to run `brew uninstall --zap pcp-perf` to remove all configuration and data files.

### 2. README.md

Update the README to clarify uninstall options for users:

```markdown
## Uninstalling

### Standard Uninstall
To uninstall PCP while preserving configuration and log files:

```bash
brew uninstall pcp-perf
```

This will:
- Stop and remove all PCP services (pmcd, pmie, pmlogger, pmproxy)
- Remove the installed package
- Leave configuration files in `/etc/pcp/` intact
- Leave data files in `/var/lib/pcp/` intact
- Leave log files in `/var/log/pcp/` intact

### Complete Uninstall
To completely remove PCP including all configuration and data files:

```bash
brew uninstall --zap pcp-perf
```

This performs a standard uninstall plus removal of all configuration, data, and log directories.

### Manual .pkg Uninstall
If you installed PCP using the .pkg installer (not Homebrew), you can use the bundled uninstall script:

**Standard Uninstall (preserves config/log files):**
```bash
sudo /usr/local/libexec/pcp/bin/uninstall-pcp
```

This will prompt for confirmation and remove PCP, leaving configuration and log files in place with a notice about their location.

**Complete Uninstall (removes everything):**
```bash
sudo /usr/local/libexec/pcp/bin/uninstall-pcp --force
```

This skips the confirmation prompt and removes all PCP files including configuration, data, and logs in `/etc/pcp/`, `/var/lib/pcp/`, and `/var/log/pcp/`.

### 3. Version Updates

When a new PCP version is released with these uninstaller improvements:

1. Update the `version` field in the cask
2. Download the new .dmg and calculate the new SHA256 checksum:
   ```bash
   shasum -a 256 pcp-X.Y.Z-BUILD.dmg
   ```
3. Update the `sha256` field in the cask
4. Test the installation and uninstallation process

## Testing Checklist for Homebrew Cask

After making these updates, test the following scenarios:

### Install Test
```bash
brew install performancecopilot/performancecopilot/pcp-perf
launchctl list | grep io.pcp  # Should show 4 services
ls -la /Library/LaunchDaemons/io.pcp.*  # Should show 4 plists
```

### Standard Uninstall Test
```bash
brew uninstall pcp-perf
launchctl list | grep io.pcp  # Should show nothing
ls -la /Library/LaunchDaemons/io.pcp.* 2>&1  # Should show "No such file"
ls -d /etc/pcp /var/lib/pcp /var/log/pcp 2>&1  # Should still exist
pkgutil --pkgs | grep pcp  # Should show nothing
```

### Zap Test
```bash
brew install performancecopilot/performancecopilot/pcp-perf
brew uninstall --zap pcp-perf
launchctl list | grep io.pcp  # Should show nothing
ls -la /Library/LaunchDaemons/io.pcp.* 2>&1  # Should show "No such file"
ls -d /etc/pcp /var/lib/pcp /var/log/pcp 2>&1  # Should show "No such file"
```

## Alignment with .pkg Uninstaller

The Homebrew cask and the .pkg uninstaller now follow the same principles:

1. **Service Management**: Both use modern `launchctl` commands to stop services
2. **Package Receipts**: Both use `pkgutil --forget` for modern macOS
3. **Config/Data Preservation**: Both leave configuration and log files in place by default
4. **User Choice**:
   - .pkg users can manually delete directories after uninstall
   - Homebrew users can use `--zap` for automatic cleanup
5. **Complete Cleanup**: Both remove all LaunchDaemons plists

## Notes

- The Homebrew cask handles the `pkgutil` and `launchctl` operations automatically
- The .pkg uninstaller requires manual execution with sudo
- Both methods are now fully compatible and can cleanly uninstall PCP
