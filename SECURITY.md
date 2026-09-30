# Security policy

## Supported versions

Only the latest release gets security fixes. If you are on an older version, update first and check whether the problem is still there. A fix may also land on `master` before it is released.

## Reporting a vulnerability

Please report security problems privately, not in a public issue or discussion. There are two ways:

1. On GitHub, open the repository's **Security** tab and press **Report a vulnerability** (private vulnerability reporting). This keeps the report and any discussion between you and the maintainer.
2. Email komsky@gmail.com with "Lupka security" in the subject.

If you are not sure whether something counts as a security issue, send it privately anyway. It is easy to make it public later and impossible to take it back.

### What to include

- The Lupka version (`lupka --version`) and how you installed it (release `.deb`, built from source).
- Your distribution, desktop and its version, and whether the session is Wayland or X11. Many paths in Lupka depend on these.
- What you found, what an attacker could do with it, and what they need first (a local account, a running Lupka, a file or clipboard they control).
- Steps or a small proof of concept that reproduces it.
- A suggested fix, if you have one.
- Whether you want to be credited, and under what name.

## What is in scope

- **The D-Bus service.** The daemon owns `io.github.komsky.Lupka` on the session bus and exposes `Trigger(action)` and `Version()`. Reports about what a caller can make the daemon do, including whether a sandboxed application that can reach the service could capture or record the screen without the prompts the desktop would normally show, are in scope. So are crashes or misbehaviour caused by malformed input to `Trigger`.
- **Files Lupka writes.** Screenshots saved from the snip and draw modes, recordings (by default in `~/Videos`), the settings file `~/.config/lupka/lupka.ini` (which also holds the portal restore tokens for screen capture, recording and DemoType), the login entry in `~/.config/autostart/`, and the desktop entry written to `~/.local/share/applications/` on KDE. Problems such as writing outside the intended directory, following symlinks, overwriting files the user did not choose, or creating entries that run something other than Lupka are in scope.
- **DemoType scripts.** Lupka reads a script file chosen in Settings, and can also take a script from the clipboard when the text starts with `[start]`. It then types into whatever window has focus. Bugs in the parser, keystrokes the script does not ask for, and ways for another program to get text typed through the clipboard script feature without the user deciding so are in scope.
- **The Debian package** built from this repository: file contents, permissions and metadata.

## What is out of scope

- Bugs in Qt, GStreamer, PipeWire, xdg-desktop-portal, the compositor or the desktop itself. Please report those upstream. If Lupka's way of using one of them makes the problem worse, tell me as well.
- Lupka seeing the screen. That is what it is for, and any program running as your user can already take screenshots on X11. On Wayland, the desktop's own permission prompts apply where it has them.
- Attacks that need root, or write access to your home directory, or to Lupka's own binary.
- Denial of service against your own session by something you ran yourself.
- Reports from automated scanners with no demonstrated impact.

## What to expect

Lupka is a small volunteer project, so the timings below are intentions, not promises.

- I will acknowledge your report within about a week.
- I will tell you whether I can reproduce it and how serious I think it is, and keep you updated while I work on a fix. How quickly that happens depends on the severity and on how much spare time I have.
- I would like to agree a disclosure date with you once a fix exists. My default is to publish the details, with credit unless you prefer not, when the fixed release is out, and no later than 90 days after your report unless we agree otherwise.
- There is no bug bounty.
