"""nofocus.py -- keep test players from taking keyboard focus away from the person using the machine.

A test player is a normal macOS app: when it starts, macOS (and Unity) bring it to the front, which steals typing from whatever the
user was doing. hand_back() remembers the app that was in front before the launch and gives the focus back to it whenever the test
player has taken it, for the first seconds of the run. Runs keep producing frames in the background because the runners use a frame
cap (VSync off).
"""
import subprocess, threading, time


def frontmost():
    """The bundle id of the app in front ("" when it cannot be read)."""
    try:
        r = subprocess.run(["osascript", "-e", 'id of application (path to frontmost application as text)'],
                           capture_output=True, text=True, timeout=5)
        return r.stdout.strip()
    except Exception:
        return ""


def is_test_player(bundle):
    """Every runner gives its player copy a bundle id of its own under local.reflex. (run.py: local.reflex.showcase.test-..., ...)."""
    return bundle.startswith("local.reflex.")


def hand_back(previous, seconds=6 * 3600.0):
    """Give the focus back whenever a TEST PLAYER took it, for the whole run (a daemon thread: it ends with the runner; `seconds` caps it), to
    the app the person was in last (`previous` until they switch). A player can grab the focus long after its launch (a resolution change, a
    scene load), so this is not limited to the first seconds.
    Only a test player in front is acted on: any other app in front is the person's own choice and becomes the one handed back to (handing
    back to the app in front at launch whenever ANY other app was in front took the focus from every window the person switched to, every
    half second, while a run was going)."""
    if not previous:
        return

    def run():
        last = previous if not is_test_player(previous) else ""
        end = time.time() + seconds
        while time.time() < end:
            time.sleep(0.5)
            now = frontmost()
            if not now:
                continue
            if not is_test_player(now):
                last = now
            elif last:
                subprocess.run(["osascript", "-e", 'tell application id "%s" to activate' % last], capture_output=True)

    threading.Thread(target=run, daemon=True).start()
