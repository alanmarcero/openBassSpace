#pragma once

#include "ParameterSnapshot.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

namespace alanmarcero::suite {

// Groups parameter edits into JUCE UndoManager transactions.
// A slider drag or a button click is one step. Host automation that does not
// arrive on the message thread is left out of the undo stack.
class SuiteHistory : private juce::AudioProcessorParameter::Listener {
public:
    using Memory = juce::ValueTree;
    using Capture = std::function<Memory()>;
    using Restore = std::function<void(const Memory&)>;

    SuiteHistory(juce::AudioProcessor& processor, Capture capture, Restore restore);
    ~SuiteHistory() override;

    juce::UndoManager undo;

    void performMutation(const juce::String& name, const std::function<void()>& mutate);
    void clear();

    [[nodiscard]] bool isApplying() const noexcept { return applying_; }

private:
    class SnapshotAction;

    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;
    void commit(const juce::String& name, const Memory& before, const Memory& after);
    [[nodiscard]] bool onMessageThread() const;

    juce::AudioProcessor& processor_;
    Capture capture_;
    Restore restore_;
    Memory gestureStart_;
    int gestureDepth_ = 0;
    bool applying_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SuiteHistory)
};

} // namespace alanmarcero::suite
