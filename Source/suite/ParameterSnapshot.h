#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <utility>
#include <vector>

namespace alanmarcero::suite {

// Denormalised parameter values keyed by parameter ID. Order is part of equality
// for undo snapshots, so callers should build it from the processor's parameter list.
using ParameterSnapshot = std::vector<std::pair<juce::String, float>>;

inline juce::ValueTree snapshotToTree(const juce::Identifier& type, const ParameterSnapshot& values) {
    juce::ValueTree tree(type);
    for (const auto& [id, value] : values) {
        juce::ValueTree child("P");
        child.setProperty("id", id, nullptr);
        child.setProperty("value", static_cast<double>(value), nullptr);
        tree.appendChild(child, nullptr);
    }
    return tree;
}

inline ParameterSnapshot snapshotFromTree(const juce::ValueTree& tree) {
    ParameterSnapshot values;
    values.reserve(static_cast<size_t>(tree.getNumChildren()));
    for (int i = 0; i < tree.getNumChildren(); ++i) {
        const auto child = tree.getChild(i);
        if (!child.hasType("P")) {
            continue;
        }
        values.emplace_back(child.getProperty("id").toString(),
                            static_cast<float>(static_cast<double>(child.getProperty("value"))));
    }
    return values;
}

} // namespace alanmarcero::suite
