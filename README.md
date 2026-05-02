# PAMDockCheck

A PAM module for the Framework 13 laptop on Fedora that detects whether an
external display is connected. When docked (display connected), fingerprint
authentication is skipped and sudo asks for your password instead. When
running on the go (no external display), fingerprint works as normal.

Tested on: Framework 13, Fedora Workstation 44, authselect `local` profile.

---

## How it works

The module reads `/sys/class/drm/card1-DP-*/status`. On the Framework 13,
external displays connect via USB-C expansion cards which the kernel exposes
as DisplayPort connectors (`DP-1` through `DP-8`). The built-in screen
(`eDP-1`) and virtual connectors (`Writeback-1`) are ignored.

- External display connected → `PAM_SUCCESS` → PAM jumps over `pam_fprintd.so` → password prompt
- No external display → `PAM_IGNORE` → PAM falls through to `pam_fprintd.so` → fingerprint prompt

---

## Makefile

The Makefile handles the full lifecycle of the module:

- `make` - compiles `pam_dock_check.c` into `pam_dock_check.so`
- `make install` - installs the `.so` to `/usr/lib64/security/` (Fedora default PAM module path)
- `make uninstall` - removes the `.so` from `/usr/lib64/security/`
- `make clean` - removes the locally built `.so` from the project folder

---

## READ THIS FIRST

### 1. Test external display detection manually to verify this module will work.

```bash
for f in /sys/class/drm/card1-DP-*/status; do echo "$f: $(cat $f)"; done
```

When docked at least one line should read `connected`. When disconnected all should read `disconnected`.

### 2. **Open a terminal and get a root shell first as a safety net before following any of the installation steps:**

```bash
sudo -s
```
Since you are working with authentication module, if you follow any of the steps wrong there is a chance of locking yourself out of running sudo. This is a real risk, it happened during development of this module.

Follow the installation steps in a new terminal while keeping this one open.

### 3. In the case you lock yourself out of sudo, in the opened superuser terminal:

```bash
authselect select $(cat ./authselect_profile.txt) --force
```

authselect_profile.txt is created in step 4 of installation.

---

## Installation

### 1. Clone this repo and change directory

```bash
git clone https://github.com/AalmanSadath/PAMDockCheck.git
cd PAMDockCheck
```

### 2. Install build dependencies

```bash
sudo dnf install gcc make pam-devel
```

### 3. Build and install the module

```bash
make
sudo make install
sudo restorecon -v /usr/lib64/security/pam_dock_check.so
```

### 4. Check your current authselect profile

```bash
authselect current --raw
```

You will see something like:

```
local with-silent-lastlog with-mdns4 with-fingerprint
```

Note the full output as you will need it in step 5 and step 7 and when uninstalling this module. If `with-fingerprint` is
not listed, fingerprint auth is not enabled and this module is not needed.

Save the current authselect profile to a text file for future reference.

```bash
authselect current --raw > ./authselect_profile.txt
```

### 5. Create a custom authselect profile

Fedora manages PAM config through authselect. You must use a custom profile
so your changes survive system updates.

```bash
sudo authselect create-profile dockprofile --base-on $(cat ./authselect_profile.txt | sed 's/ .*//')
```

`$(cat ./authselect_profile.txt | sed 's/ .*//')` gets the name of your current profile from authselect_profile.txt (removes everything after the first word).

### 6. Edit the system-auth template

```bash
sudo nano /etc/authselect/custom/dockprofile/system-auth
```

Find this line:

```
auth sufficient pam_fprintd.so {include if "with-fingerprint"}
```

Insert a new line immediately above it:

```
auth [success=1 default=ignore] pam_dock_check.so {include if "with-fingerprint"}
```

So the section looks like this:

```
auth [success=1 default=ignore] pam_dock_check.so {include if "with-fingerprint"}
auth sufficient                 pam_fprintd.so    {include if "with-fingerprint"}
```

Save and exit (`Ctrl+X`, `Y`, `Enter` in nano).

### 7. Activate the custom profile

Select custom profile using the exact feature flags from step 4:

```bash
sudo authselect select custom/dockprofile $(cat ./authselect_profile.txt | sed 's/^[^ ]* //') --force
```

`$(cat ./authselect_profile.txt | sed 's/^[^ ]* //')` extracts the exact flag from authselect_profile.txt (removes the first word).

### 8. Verify the rendered PAM config

```bash
cat /etc/pam.d/system-auth
```

The auth section should look like this (make sure pam_fprintd.so is directly under pam_dock_check.so):

```
auth        required                                     pam_env.so
auth        required                                     pam_faildelay.so delay=2000000
auth        [success=1 default=ignore]                   pam_dock_check.so
auth        sufficient                                   pam_fprintd.so
auth        sufficient                                   pam_unix.so nullok
auth        required                                     pam_deny.so
```

The jump count `success=1` skips exactly one line (fprintd) when an external
display is detected, landing on `pam_unix.so` for password auth.

### 9. Test

Open a new terminal, with your external display connected:

```bash
sudo echo "test"
# Should prompt for password, not fingerprint
```

Unplug the display and test again in a new terminal:

```bash
sudo echo "test"
# Should prompt for fingerprint
```

Watch the module's decisions in real time:

```bash
sudo journalctl -f | grep pam_dock
```

You should see lines like:

```
pam_dock_check: external display on card1-DP-2
pam_dock_check(sudo:auth): pam_dock_check: external display present – using password
```

---

## Uninstall

```bash
sudo authselect select $(cat ./authselect_profile.txt) --force
sudo rm -rf /etc/authselect/custom/dockprofile
sudo make uninstall
```

Run from Repo directory.

---


