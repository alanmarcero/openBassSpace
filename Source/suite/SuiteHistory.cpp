#include "SuiteHistory.h"

namespace alanmarcero::suite {

class SuiteHistory::SnapshotAction : public juce::UndoableAction {
public:
    SnapshotAction(SuiteHistory& owner, Memory before, Memory after)
        : owner_(owner), before_(std::move(before)), after_(std::move(after)) {}

    // The first perform() is the moment the action is recorded. The plug-in is
    // already in the "after" state, and this call can arrive from inside a
    // parameter gesture callback that still holds the listener lock.
    bool perform() override {
        if (!recorded_) {
            recorded_ = true;
            return true;
        }
        apply(after_);
        return true;
    }

    bool undo() override {
        apply(before_);
        return true;
    }

    int getSizeInUnits() override { return 1; }

private:
    void apply(const Memory& memory) const {
        const juce::ScopedValueSetter<bool> guard(owner_.applying_, true);
        owner_.restore_(memory);
    }

    SuiteHistory& owner_;
    Memory before_;
    Memory after_;
    bool recorded_ = false;
};

SuiteHistory::SuiteHistory(juce::AudioProcessor& processor, Capture capture, Restore restore)
    : processor_(processor), capture_(std::move(capture)), restore_(std::move(restore)) {
    for (auto& history : histories_) {
        history.setMaxNumberOfStoredUnits(80, 24);
    }
    for (auto* parameter : processor_.getParameters()) {
        if (parameter != nullptr) {
            parameter->addListener(this);
        }
    }
}

SuiteHistory::~SuiteHistory() {
    for (auto* parameter : processor_.getParameters()) {
        if (parameter != nullptr) {
            parameter->removeListener(this);
        }
    }
}

void SuiteHistory::clear() {
    gestureDepth_ = 0;
    gestureStart_ = {};
    for (auto& history : histories_) {
        history.clearUndoHistory();
    }
}

void SuiteHistory::selectSlot(int slot) {
    gestureDepth_ = 0;
    gestureStart_ = {};
    activeSlot_ = slot;
}

void SuiteHistory::clearSlot(int slot) {
    histories_[static_cast<size_t>(slot)].clearUndoHistory();
}

bool SuiteHistory::onMessageThread() const {
    auto* messages = juce::MessageManager::getInstanceWithoutCreating();
    return messages != nullptr && messages->isThisTheMessageThread();
}

void SuiteHistory::performMutation(const juce::String& name, const std::function<void()>& mutate) {
    if (!onMessageThread() || applying_) {
        mutate();
        return;
    }

    const auto before = capture_();
    {
        const juce::ScopedValueSetter<bool> guard(applying_, true);
        mutate();
    }
    commit(name, before, capture_());
}

void SuiteHistory::commit(const juce::String& name, const Memory& before, const Memory& after) {
    if (!before.isValid() || before.isEquivalentTo(after)) {
        return;
    }

    activeHistory().beginNewTransaction(name);
    activeHistory().perform(new SnapshotAction(*this, before.createCopy(), after.createCopy()));
}

void SuiteHistory::parameterValueChanged(int, float) {}

void SuiteHistory::parameterGestureChanged(int parameterIndex, bool gestureIsStarting) {
    if (applying_ || !onMessageThread()) {
        return;
    }

    if (gestureIsStarting) {
        if (gestureDepth_ == 0) {
            gestureStart_ = capture_();
        }
        ++gestureDepth_;
        return;
    }

    if (gestureDepth_ <= 0) {
        return;
    }

    --gestureDepth_;
    if (gestureDepth_ > 0) {
        return;
    }

    juce::String name("Edit");
    const auto& parameters = processor_.getParameters();
    if (juce::isPositiveAndBelow(parameterIndex, parameters.size())) {
        if (auto* parameter = parameters[parameterIndex]) {
            name = parameter->getName(40);
        }
    }
    const auto start = gestureStart_;
    gestureStart_ = {};
    commit(name, start, capture_());
}

} // namespace alanmarcero::suite
