#pragma once

#include <juce_core/juce_core.h>
#include <vector>

// Runs the CV pipeline (CV/run_pipeline.py: camera -> SAM mask -> wavetable +
// macros -> .ospreset) and reports the preset it produced.
//
// The pipeline is opened in a terminal window, not as a child of the host:
// the terminal owns the camera permission (macOS) and gives the pipeline a
// console for the object-name prompt. Each OS gets its own launcher:
// a .command file in Terminal (macOS), a .bat file in a cmd window (Windows),
// a .sh file in the first terminal emulator found (Linux). The script writes
// its outcome to a result file, which poll() picks up: "OK\n<preset path>",
// "ERROR\n<message>" or "CANCELLED\n<message>".
//
// The CV folder and its Python come from the build (OBJECTSYNTH_REPO_DIR, set
// by CMake to this checkout), so this only works on the machine that built it.
class CameraPipeline
{
public:
    enum class State { Idle, Running, Finished };

    struct Result
    {
        bool ok = false;
        bool cancelled = false;
        juce::File preset;
        juce::String message;
    };

    static juce::File repoDirectory()
    {
       #ifdef OBJECTSYNTH_REPO_DIR
        return juce::File(OBJECTSYNTH_REPO_DIR);
       #else
        return {};
       #endif
    }

    static juce::File cvDirectory() { return repoDirectory().getChildFile("CV"); }
    static juce::File python()
    {
        return repoDirectory().getChildFile(isWindows ? ".venv/Scripts/python.exe" : ".venv/bin/python");
    }

    bool isRunning() const noexcept { return running; }

    // Opens the pipeline in a terminal window. Returns false (with a message) when it
    // cannot start.
    bool start(juce::String& message)
    {
        const auto script = cvDirectory().getChildFile("run_pipeline.py");
        if (!script.existsAsFile())
        {
            message = "CV pipeline not found: " + script.getFullPathName()
                      + " (build the plugin from your checkout, see README.md)";
            return false;
        }
        if (!python().existsAsFile())
        {
            message = "Python not found: " + python().getFullPathName()
                      + " (create the .venv, see README.md)";
            return false;
        }

        const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("ObjectSynthCamera");
        if (!folder.createDirectory())
        {
            message = "Cannot create " + folder.getFullPathName();
            return false;
        }
        resultFile = folder.getChildFile("result.txt");
        resultFile.deleteFile();

        if (!(isWindows ? openOnWindows(folder, message)
                        : isMac ? openOnMac(folder, message)
                                : openOnLinux(folder, message)))
            return false;

        running = true;
        message = "Camera open in a terminal: Space photo, Enter save";
        return true;
    }

    void cancel() { running = false; }

    // Returns Finished once, with the result filled in, when the pipeline ends.
    State poll(Result& out)
    {
        if (!running) return State::Idle;
        if (!resultFile.existsAsFile()) return State::Running;

        running = false;
        juce::StringArray lines;
        lines.addLines(resultFile.loadFileAsString());
        const auto status = lines[0].trim();
        const auto detail = lines[1].trim();
        out.ok = status == "OK";
        out.cancelled = status == "CANCELLED";
        out.preset = out.ok ? juce::File(detail) : juce::File();
        out.message = detail.isEmpty() ? status : detail;
        return State::Finished;
    }

private:
   #if JUCE_WINDOWS
    static constexpr bool isWindows = true, isMac = false;
   #elif JUCE_MAC
    static constexpr bool isWindows = false, isMac = true;
   #else
    static constexpr bool isWindows = false, isMac = false;
   #endif

    // POSIX shell single quotes.
    static juce::String quote(const juce::String& s)
    {
        return "'" + s.replace("'", "'\\''") + "'";
    }

    // Body of a POSIX shell launcher (zsh on macOS, sh on Linux). When the
    // pipeline fails it waits for a key, so the error stays readable;
    // otherwise the window closes (Linux terminals do it when the script ends,
    // Terminal needs `closeWindow`).
    juce::String shellScript(const juce::String& shell, const juce::String& closeWindow = {}) const
    {
        const auto result = quote(resultFile.getFullPathName());
        return "#!" + shell + "\n"
               "cd " + quote(cvDirectory().getFullPathName()) + " || exit 1\n"
               + quote(python().getFullPathName()) + " run_pipeline.py --result-file " + result + "\n"
               "status=$?\n"
               "[ -f " + result + " ] || printf 'ERROR\\nThe pipeline did not finish.\\n' > " + result + "\n"
               "if [ $status -ne 0 ]; then printf '\\nPress Enter to close.'; read _; exit $status; fi\n"
               + closeWindow;
    }

    static bool writeLauncher(const juce::File& file, const juce::String& text, juce::String& message)
    {
        if (file.replaceWithText(text, false, false, isWindows ? "\r\n" : "\n")
            && (isWindows || file.setExecutePermission(true)))
            return true;
        message = "Cannot write " + file.getFullPathName();
        return false;
    }

    bool openOnMac(const juce::File& folder, juce::String& message)
    {
        const auto launcher = folder.getChildFile("ObjectSynth Camera.command");
        // Terminal keeps a window open after its script ends: close ours (found
        // by its tty) from a detached osascript, once the shell has exited, so
        // Terminal does not ask about running processes.
        const juce::String closeWindow =
            "nohup osascript -e 'on run argv' -e 'delay 0.5' -e 'tell application \"Terminal\"'"
            " -e 'repeat with w in windows' -e 'if tty of tab 1 of w is (item 1 of argv) then close w'"
            " -e 'end repeat' -e 'end tell' -e 'end run' \"$(tty)\" >/dev/null 2>&1 &!\n"
            "exit 0\n";
        if (!writeLauncher(launcher, shellScript("/bin/zsh", closeWindow), message)) return false;

        juce::ChildProcess open;
        if (!open.start(juce::StringArray { "/usr/bin/open", "-a", "Terminal",
                                            launcher.getFullPathName() }, 0))
        {
            message = "Cannot open Terminal";
            return false;
        }
        open.waitForProcessToFinish(5000);
        return true;
    }

    bool openOnWindows(const juce::File& folder, juce::String& message)
    {
        const auto launcher = folder.getChildFile("ObjectSynthCamera.bat");
        const auto q = [](const juce::File& f) { return f.getFullPathName().quoted(); };
        const auto text =
            "@echo off\n"
            "cd /d " + q(cvDirectory()) + " || exit /b 1\n"
            + q(python()) + " run_pipeline.py --result-file " + q(resultFile) + "\n"
            "set status=%errorlevel%\n"
            "if not exist " + q(resultFile) + " (echo ERROR& echo The pipeline did not finish.)> " + q(resultFile) + "\n"
            "if not %status%==0 pause\n"
            "exit\n"; // `start` runs a .bat with cmd /K: close the window
        if (!writeLauncher(launcher, text, message)) return false;

        // `start` opens a new console window. Its first argument is the title
        // only when quoted: JUCE quotes arguments with spaces, so keep one.
        juce::ChildProcess open;
        if (!open.start(juce::StringArray { "cmd.exe", "/c", "start", "ObjectSynth Camera",
                                            launcher.getFullPathName() }, 0))
        {
            message = "Cannot open a command prompt";
            return false;
        }
        open.waitForProcessToFinish(5000);
        return true;
    }

    bool openOnLinux(const juce::File& folder, juce::String& message)
    {
        const auto launcher = folder.getChildFile("objectsynth-camera.sh");
        if (!writeLauncher(launcher, shellScript("/bin/sh"), message)) return false;
        const auto script = launcher.getFullPathName();

        // $TERMINAL first, then common emulators, each with its "run this" flag.
        struct Emulator { juce::String name, flag; };
        std::vector<Emulator> emulators;
        const auto preferred = juce::SystemStats::getEnvironmentVariable("TERMINAL", {});
        if (preferred.isNotEmpty()) emulators.push_back({ preferred, "-e" });
        for (const auto& e : { Emulator { "x-terminal-emulator", "-e" }, Emulator { "gnome-terminal", "--" },
                               Emulator { "konsole", "-e" }, Emulator { "xfce4-terminal", "-x" },
                               Emulator { "kitty", "" }, Emulator { "alacritty", "-e" },
                               Emulator { "xterm", "-e" } })
            emulators.push_back(e);

        for (const auto& emulator : emulators)
        {
            const auto program = findProgram(emulator.name);
            if (program == juce::File()) continue;
            juce::StringArray command { program.getFullPathName() };
            if (emulator.flag.isNotEmpty()) command.add(emulator.flag);
            command.add(script);
            juce::ChildProcess open;
            if (open.start(command, 0)) return true;
        }
        message = "No terminal found: set $TERMINAL or install xterm";
        return false;
    }

    // Looks a program up in $PATH (or takes it as is when it is a path).
    static juce::File findProgram(const juce::String& name)
    {
        if (juce::File::isAbsolutePath(name))
            return juce::File(name).existsAsFile() ? juce::File(name) : juce::File();
        juce::StringArray directories;
        directories.addTokens(juce::SystemStats::getEnvironmentVariable("PATH", "/usr/bin:/bin"), ":", "");
        for (const auto& directory : directories)
        {
            if (!juce::File::isAbsolutePath(directory)) continue;
            const auto candidate = juce::File(directory).getChildFile(name);
            if (candidate.existsAsFile()) return candidate;
        }
        return {};
    }

    juce::File resultFile;
    bool running = false;
};
