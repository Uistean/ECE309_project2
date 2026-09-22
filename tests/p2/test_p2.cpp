
#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>

#include <cassert>

#define TEST(name) static void name()
#define RUN(name) do { name(); std::cout << "PASS " #name "\n"; } while (0)

class LineInput : public InputSource {
public:
    explicit LineInput(const std::string& text) : in_(text) {}
    std::string read_line() override {
        std::string line;
        std::getline(in_, line);
        eof_ = in_.eof();
        return line;
    }
    bool is_eof() const override { return eof_; }
private:
    std::istringstream in_;
    bool eof_ = false;
};

class CaptureOutput : public OutputSink {
public:
    std::string text;
    void write(std::string_view t) override { text += t; }
};

static void write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
}

static const char* role_name(Role r) {
    switch (r) {
        case Role::System: return "system";
        case Role::User: return "user";
        case Role::Assistant: return "assistant";
    }
    return "assistant";
}

static void save_conv(const Conversation& conv, const std::string& path) {
    std::ofstream f(path);
    bool first = true;
    for (const Message& m : conv) {
        if (!first) f << "---\n";
        first = false;
        f << "role: " << role_name(m.role()) << "\n" << m.content() << "\n";
    }
}
 
TEST(HarnessStopsAtSentinel) {
    // chunk sizes 1..26 split the sentinel at every possible position
    for (int chunk = 1; chunk <= 26; ++chunk) {
        write_file("tmp_sentinel.script",
            "chunk: " + std::to_string(chunk) + "\n"
            "role: assistant\nBye.<|end_conversation|>\n---\n"
            "role: assistant\nshould never run\n");

        HarnessConfig cfg;
        Harness h(std::make_unique<ScriptedModelClient>("tmp_sentinel.script"), cfg);
        LineInput in("hi\nsecond\n");
        CaptureOutput out;

        StopReason r = h.run(in, out);
        assert(r.kind == StopReason::Kind::Sentinel);
        assert(out.text.find("Bye.") != std::string::npos);
        assert(out.text.find("<|") == std::string::npos);   // sentinel never printed
        assert(out.text.find("|>") == std::string::npos);
        assert(h.conversation().size() == 2);                // stopped after one turn
        assert(h.conversation().at(1).content() == "Bye.<|end_conversation|>");
    }
    std::remove("tmp_sentinel.script");
}


TEST(ScannerCatchesSentinelAtEveryBoundary) {
    const std::string sentinel = "<|end_conversation|>";
    const std::string text = "Goodbye." + sentinel;

    // whole text split into two chunks, at every possible point
    for (std::size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner scanner(sentinel);
        auto out1 = scanner.feed(text.substr(0, split));
        auto out2 = scanner.feed(text.substr(split));
        assert((out1.sentinel_found || out2.sentinel_found) &&
               "sentinel must be caught regardless of split point");
        assert(out1.safe_text + out2.safe_text == "Goodbye.");
    }

    // one character at a time
    SentinelScanner scanner(sentinel);
    std::string printed;
    bool found = false;
    for (char ch : text) {
        auto out = scanner.feed(std::string(1, ch));
        printed += out.safe_text;
        if (out.sentinel_found) { found = true; break; }
    }
    assert(found);
    assert(printed == "Goodbye.");
}

TEST(ScannerFalseAlarms) {
    const std::string sentinel = "<|end_conversation|>";
    const std::string inputs[] = {
        "<|end_world|>",
        "<|end_conversation|",      // missing the final '>'
        "<|end_conversation",
        "<<<|end_ hello |>",
    };
    for (const std::string& text : inputs) {
        SentinelScanner scanner(sentinel);
        auto a = scanner.feed(text);
        auto b = scanner.flush();
        assert(!a.sentinel_found);
        assert(!b.sentinel_found);
        assert(a.safe_text + b.safe_text == text);   // nothing lost, nothing changed
    }
}

TEST(ScannerBoundedMemory) {
    const std::string sentinel = "<|end_conversation|>";
    SentinelScanner scanner(sentinel);

    const std::string pattern = "<|end_";             // adversarial: looks like a sentinel start, never completes
    const std::size_t total = 4 * 1024 * 1024;         // 4 MB, one byte at a time
    std::size_t emitted = 0;

    for (std::size_t fed = 1; fed <= total; ++fed) {
        char ch = pattern[(fed - 1) % pattern.size()];
        auto out = scanner.feed(std::string(1, ch));
        assert(!out.sentinel_found);
        emitted += out.safe_text.size();
        // characters still held back = fed - emitted, and that is pending_'s size
        assert(fed - emitted <= sentinel.size() - 1);
    }

    auto rest = scanner.flush();
    emitted += rest.safe_text.size();
    assert(emitted == total);                          // every byte came out exactly once
}

TEST(EmptyConversation) {
    Conversation c;
    assert(c.size() == 0);
    assert(c.begin() == c.end());

    bool threw = false;
    try {
        c.at(0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    assert(threw);
}

TEST(ScannerCleanText) {
    SentinelScanner s("<|end_conversation|>");
    auto a = s.feed("hello world, this is plain text");
    auto b = s.flush();
    assert(!a.sentinel_found);
    assert(a.safe_text + b.safe_text == "hello world, this is plain text");
} 
TEST(SystemMessageStaysFirst) {
    Conversation c;
    c.append(Message(Role::System, "Be concise."));
    for (int i = 0; i < 20; i++) {
        c.append(Message(Role::User, "msg " + std::to_string(i)));
    }
    assert(c.at(0).role() == Role::System);
    assert(c.at(0).content() == "Be concise.");
    assert(c.size() == 21);
}

TEST(CopyIsDeep) {
    Conversation a;
    a.append(Message(Role::User, "hi"));
    a.append(Message(Role::Assistant, "hello"));

    Conversation b = a;
    assert(b.begin() != a.begin());          // different blocks
    assert(b.size() == 2);
    assert(b.at(1).content() == "hello");    // same contents

    a.append(Message(Role::User, "more"));   // changing a must not affect b
    assert(a.size() == 3);
    assert(b.size() == 2);
}

TEST(CopyAssignmentIsDeep) {
    Conversation a;
    a.append(Message(Role::User, "hi"));
    Conversation b;
    b.append(Message(Role::User, "old"));
    b.append(Message(Role::User, "old2"));

    b = a;
    assert(b.begin() != a.begin());
    assert(b.size() == 1);
    assert(b.at(0).content() == "hi");

    b = b;                                   // self-assignment must not break anything
    assert(b.size() == 1);
    assert(b.at(0).content() == "hi");
}

TEST(MoveStealsAndEmptiesSource) {
    Conversation a;
    a.append(Message(Role::User, "hi"));
    a.append(Message(Role::User, "yo"));
    const Message* p = a.begin();

    Conversation b = std::move(a);
    assert(b.begin() == p);                  // stole the same block
    assert(b.size() == 2);
    assert(a.size() == 0);                   // source is empty
    assert(a.begin() == nullptr);

    a.append(Message(Role::User, "reuse"));  // source is still usable
    assert(a.size() == 1);
}

TEST(MoveAssignmentStealsAndEmptiesSource) {
    Conversation a;
    a.append(Message(Role::User, "hi"));
    const Message* p = a.begin();
    Conversation b;
    b.append(Message(Role::User, "old"));

    b = std::move(a);
    assert(b.begin() == p);
    assert(b.size() == 1);
    assert(b.at(0).content() == "hi");
    assert(a.size() == 0);
    assert(a.begin() == nullptr);
}

TEST(GrowthDoublesAndKeepsData) {
    Conversation c;
    int reallocations = 0;
    const Message* last = c.begin();
    for (int i = 0; i < 100; i++) {
        c.append(Message(Role::User, "m" + std::to_string(i)));
        if (c.begin() != last) {
            reallocations++;
            last = c.begin();
        }
    }
    assert(c.size() == 100);
    assert(reallocations == 8);              // capacities 1,2,4,8,16,32,64,128
    for (int i = 0; i < 100; i++) {
        assert(c.at(i).content() == "m" + std::to_string(i));
    }
}
int main() {
    RUN(EmptyConversation);
    RUN(ScannerCleanText);
    RUN(SystemMessageStaysFirst);
    RUN(CopyIsDeep);
    RUN(CopyAssignmentIsDeep);
    RUN(MoveStealsAndEmptiesSource);
    RUN(MoveAssignmentStealsAndEmptiesSource);
    RUN(GrowthDoublesAndKeepsData);
    RUN(ScannerCatchesSentinelAtEveryBoundary);
    RUN(ScannerFalseAlarms);
    RUN(ScannerBoundedMemory);
    RUN(HarnessStopsAtSentinel);
    std::cout << "All tests passed\n";
}