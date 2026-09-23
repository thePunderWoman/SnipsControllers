import subprocess

Import("env")

# Firmware version shown on the on-device Device Info screen (see
# menu.cpp's kDeviceInfo render case) — the repo has no tag-based release
# process yet, so this is just the current commit, which is still exactly
# what you want on a bring-up device: proof of what's actually flashed.
# Once real version tags exist, `git describe` picks them up automatically
# with no code changes needed here.
def firmware_version():
    try:
        return (
            subprocess.check_output(
                ["git", "describe", "--tags", "--always", "--dirty"],
                cwd=env["PROJECT_DIR"],
                stderr=subprocess.DEVNULL,
            )
            .decode()
            .strip()
        )
    except Exception:
        return "unknown"


env.Append(CPPDEFINES=[("FIRMWARE_VERSION", '\\"%s\\"' % firmware_version())])
