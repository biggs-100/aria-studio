#include <catch2/catch_test_macros.hpp>
#include "midi/midi_graph.h"
#include "midi/midi_node_types.h"

#include <memory>
#include <vector>

using namespace aria;

TEST_CASE("MidiGraph topology — add and remove nodes", "[midi][graph]") {
    MidiGraph graph;

    SECTION("starts empty") {
        REQUIRE(graph.empty());
        REQUIRE(graph.node_count() == 0);
    }

    SECTION("add_node increases node count") {
        auto input = std::make_unique<MidiInputNode>();
        auto id = graph.add_node(std::move(input));

        REQUIRE_FALSE(graph.empty());
        REQUIRE(id != kInvalidMidiNodeID);
        REQUIRE(graph.get_node(id) != nullptr);
    }

    SECTION("remove_node decreases node count") {
        auto id = graph.add_node(std::make_unique<MidiInputNode>());
        graph.remove_node(id);
        REQUIRE(graph.get_node(id) == nullptr);
    }

    SECTION("add_node returns valid IDs") {
        auto id1 = graph.add_node(std::make_unique<MidiInputNode>());
        auto id2 = graph.add_node(std::make_unique<MidiInputNode>());

        REQUIRE(id1 != id2);
        REQUIRE(id1 != kInvalidMidiNodeID);
        REQUIRE(id2 != kInvalidMidiNodeID);
    }

    SECTION("add_node with nullptr returns invalid ID") {
        auto id = graph.add_node(nullptr);
        REQUIRE(id == kInvalidMidiNodeID);
    }
}

TEST_CASE("MidiGraph connect/disconnect", "[midi][graph]") {
    MidiGraph graph;

    auto input_id  = graph.add_node(std::make_unique<MidiInputNode>());
    auto output_id = graph.add_node(std::make_unique<MidiOutputNode>());

    SECTION("connect creates a link") {
        graph.connect(input_id, output_id);
        // Process should succeed without errors
        graph.process(128, 0);
    }

    SECTION("disconnect removes a link") {
        graph.connect(input_id, output_id);
        graph.disconnect(input_id, output_id);
        graph.process(128, 0);  // should still succeed
    }

    SECTION("connect to invalid ID is safe") {
        graph.connect(input_id, 999);
        graph.process(128, 0);
    }

    SECTION("disconnect non-existent connection is safe") {
        graph.disconnect(input_id, output_id);
        graph.process(128, 0);
    }
}

TEST_CASE("MidiGraph cycle detection", "[midi][graph]") {
    MidiGraph graph;

    auto n1 = graph.add_node(std::make_unique<MidiRouterNode>());
    auto n2 = graph.add_node(std::make_unique<MidiRouterNode>());

    SECTION("self-connection is a cycle") {
        REQUIRE(graph.would_create_cycle(n1, n1));
    }

    SECTION("simple forward connection is not a cycle") {
        REQUIRE_FALSE(graph.would_create_cycle(n1, n2));
    }

    SECTION("backwards connection is a cycle") {
        graph.connect(n1, n2);
        REQUIRE(graph.would_create_cycle(n2, n1));
    }
}

TEST_CASE("MidiGraph processing", "[midi][graph]") {
    MidiGraph graph;

    auto input  = graph.add_node(std::make_unique<MidiInputNode>());
    auto output = graph.add_node(std::make_unique<MidiOutputNode>());
    graph.connect(input, output);

    SECTION("process with empty events succeeds") {
        graph.process(128, 0);
        // No crash is the test
    }

    SECTION("process multiple times succeeds") {
        graph.process(128, 0);
        graph.process(128, 128);
        graph.process(256, 256);
    }

    SECTION("process on empty graph is safe") {
        MidiGraph empty_graph;
        empty_graph.process(128, 0);
    }
}

TEST_CASE("MidiGraph with MidiTransformerNode", "[midi][graph]") {
    MidiGraph graph;

    auto input = graph.add_node(std::make_unique<MidiInputNode>());
    auto transformer = graph.add_node(std::make_unique<MidiTransformerNode>());
    auto output = graph.add_node(std::make_unique<MidiOutputNode>());

    graph.connect(input, transformer);
    graph.connect(transformer, output);

    SECTION("process with transformer succeeds") {
        graph.process(128, 0);
    }

    SECTION("transpose set before process") {
        auto* trans_node = dynamic_cast<MidiTransformerNode*>(graph.get_node(transformer));
        REQUIRE(trans_node != nullptr);
        trans_node->set_transpose(5);
        graph.process(128, 0);
    }
}

TEST_CASE("MidiGraph disconnect_all", "[midi][graph]") {
    MidiGraph graph;

    auto n1 = graph.add_node(std::make_unique<MidiInputNode>());
    auto n2 = graph.add_node(std::make_unique<MidiRouterNode>());
    auto n3 = graph.add_node(std::make_unique<MidiOutputNode>());

    graph.connect(n1, n2);
    graph.connect(n2, n3);

    SECTION("disconnect_all removes all connections from a node") {
        graph.disconnect_all(n2);
        graph.process(128, 0);  // should succeed
    }

    SECTION("removing a node implicitly disconnects it") {
        graph.remove_node(n2);
        graph.process(128, 0);  // should succeed
        REQUIRE(graph.get_node(n2) == nullptr);
    }
}

TEST_CASE("MidiGraph rebuild on dirty", "[midi][graph]") {
    MidiGraph graph;

    // Adding nodes marks graph dirty
    auto n1 = graph.add_node(std::make_unique<MidiInputNode>());
    auto n2 = graph.add_node(std::make_unique<MidiOutputNode>());
    graph.connect(n1, n2);

    // Process triggers rebuild
    graph.process(128, 0);

    // Add a node mid-stream — should trigger another rebuild on next process
    auto n3 = graph.add_node(std::make_unique<MidiRouterNode>());
    graph.connect(n1, n3);
    graph.connect(n3, n2);

    graph.process(128, 0);
    // If we get here, rebuild succeeded
}

TEST_CASE("MidiGraph complex node types", "[midi][graph]") {
    MidiGraph graph;

    auto input  = graph.add_node(std::make_unique<MidiInputNode>());
    auto router = graph.add_node(std::make_unique<MidiRouterNode>());
    auto trans  = graph.add_node(std::make_unique<MidiTransformerNode>());
    auto output = graph.add_node(std::make_unique<MidiOutputNode>());

    SECTION("input → router → transformer → output") {
        graph.connect(input, router);
        graph.connect(router, trans);
        graph.connect(trans, output);

        graph.process(128, 0);
    }

    SECTION("dynamic_cast to node types works") {
        auto* trans_node = dynamic_cast<MidiTransformerNode*>(graph.get_node(trans));
        REQUIRE(trans_node != nullptr);
        REQUIRE(trans_node->label() == std::string("MidiTransformer"));

        auto* input_node = dynamic_cast<MidiInputNode*>(graph.get_node(input));
        REQUIRE(input_node != nullptr);
        REQUIRE(input_node->label() == std::string("MidiInput"));

        auto* router_node = dynamic_cast<MidiRouterNode*>(graph.get_node(router));
        REQUIRE(router_node != nullptr);
        REQUIRE(router_node->label() == std::string("MidiRouter"));
    }
}

TEST_CASE("MidiGraph process event flow", "[midi][graph]") {
    MidiGraph graph;

    auto input  = graph.add_node(std::make_unique<MidiInputNode>());
    auto output = graph.add_node(std::make_unique<MidiOutputNode>());
    graph.connect(input, output);

    // Push some events into the input node
    auto* input_node = dynamic_cast<MidiInputNode*>(graph.get_node(input));
    REQUIRE(input_node != nullptr);

    std::vector<MidiEvent> events;
    MidiEvent ev;
    ev.type = MidiMessageType::NoteOn;
    ev.channel = 0;
    ev.data1 = 60;
    ev.data2 = 100;
    events.push_back(ev);

    input_node->push_events(events);

    // Process the graph
    graph.process(128, 0);

    // Events should have flowed to the output node
    auto* output_node = dynamic_cast<MidiOutputNode*>(graph.get_node(output));
    REQUIRE(output_node != nullptr);

    auto drained = output_node->drain_output();
    REQUIRE(drained.size() == 1);
    REQUIRE(drained[0].type == MidiMessageType::NoteOn);
    REQUIRE(drained[0].data1 == 60);
}

TEST_CASE("MidiGraph transformer modifies events", "[midi][graph]") {
    MidiGraph graph;

    auto input  = graph.add_node(std::make_unique<MidiInputNode>());
    auto trans  = graph.add_node(std::make_unique<MidiTransformerNode>());
    auto output = graph.add_node(std::make_unique<MidiOutputNode>());
    graph.connect(input, trans);
    graph.connect(trans, output);

    // Set transformer to transpose +5 and remap to channel 2
    auto* trans_node = dynamic_cast<MidiTransformerNode*>(graph.get_node(trans));
    trans_node->set_transpose(5);
    trans_node->set_channel_remap(2);

    // Push a note
    auto* input_node = dynamic_cast<MidiInputNode*>(graph.get_node(input));
    std::vector<MidiEvent> events;
    MidiEvent ev;
    ev.type = MidiMessageType::NoteOn;
    ev.channel = 0;
    ev.data1 = 60;
    ev.data2 = 100;
    events.push_back(ev);
    input_node->push_events(events);

    // Process
    graph.process(128, 0);

    // Check output
    auto* output_node = dynamic_cast<MidiOutputNode*>(graph.get_node(output));
    auto drained = output_node->drain_output();

    REQUIRE(drained.size() == 1);
    REQUIRE(drained[0].data1 == 65);      // 60 + 5
    REQUIRE(drained[0].channel == 2);     // remapped to channel 2
}
