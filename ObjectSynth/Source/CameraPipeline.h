#pragma once

#include <juce_core/juce_core.h>

// Runs the CV pipeline (CV/run_pipeline.py: camera -> SAM mask -> wavetable +
// macros -> .ospreset) and reports the preset it produced.
//
// The pipeline is opened in Terminal, not as a child of the host: Terminal
// owns the camera permission and gives the pipeline a console for the
// object-name prompt. The script writes its outcome to a result file, which
// poll() picks up: "OK\n<preset path>", "ERROR\n<message>" or
// "CANCELLED\n<message>".
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
    static juce::File python() { return repoDirectory().getChildFile(".venv/bin/python"); }

    bool isRunning() const noexcept { return running; }

    // Opens the pipeline in Terminal. Returns false (with a message) when it
    // cannot start.
    bool start(juce::String& message)
    {
        const auto script = cvDirectory().getChildFile("run_pipeline.py");
        if (!script.existsAsFile())
        {
            message = "CV pipeline not found: " + script.getFullPathName();
            return false;
        }
        if (!python().existsAsFile())
        {
            message = "Python not found: " + python().getFullPathName();
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

        const auto launcher = folder.getChildFile("ObjectSynth Camera.command");
        const auto result = quote(resultFile.getFullPathName());
        const juce::String text =
            "#!/bin/zsh\n"
            "cd " + quote(cvDirectory().getFullPathName()) + " || exit 1\n"
            + quote(python().getFullPathName()) + " run_pipeline.py --result-file " + result + "\n"
            "[ -f " + result + " ] || printf 'ERROR\\nThe pipeline did not finish.\\n' > " + result + "\n";
        if (!launcher.replaceWithText(text, false, false, "\n") || !launcher.setExecutePermission(true))
        {
            message = "Cannot write " + launcher.getFullPathName();
            return false;
        }

        juce::ChildProcess open;
        if (!open.start(juce::StringArray { "/usr/bin/open", "-a", "Terminal",
                                            launcher.getFullPathName() }, 0))
        {
            message = "Cannot open Terminal";
            return false;
        }
        open.waitForProcessToFinish(5000);
        running = true;
        message = "Camera open in Terminal: Space photo, Enter save";
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
    static juce::String quote(const juce::String& s)
    {
        return "'" + s.replace("'", "'\\''") + "'";
    }

    juce::File resultFile;
    bool running = false;
};
