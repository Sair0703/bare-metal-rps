// test_protocol.cpp - Host-side unit tests for the game rules and the
// board-to-board protocol. Runs on a PC: no hardware needed.
#include <cstdio>
#include "protocol.h"

using namespace rps;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

static void test_rules() {
    const Move R = Move::Rock, P = Move::Paper, S = Move::Scissors;
    CHECK(judge(R, S) == Outcome::Win);
    CHECK(judge(P, R) == Outcome::Win);
    CHECK(judge(S, P) == Outcome::Win);
    CHECK(judge(S, R) == Outcome::Lose);
    CHECK(judge(R, P) == Outcome::Lose);
    CHECK(judge(P, S) == Outcome::Lose);
    CHECK(judge(R, R) == Outcome::Tie);
    CHECK(judge(P, P) == Outcome::Tie);
    CHECK(judge(S, S) == Outcome::Tie);
    CHECK(nextMove(R) == P && nextMove(P) == S && nextMove(S) == R);
}

static bool feed_all(FrameParser& p, const uint8_t* bytes, int n, Frame& out) {
    bool got = false;
    for (int i = 0; i < n; ++i) got = p.feed(bytes[i], out) || got;
    return got;
}

static void test_round_trip() {
    FrameParser p;
    uint8_t buf[kFrameLen];
    encode(Frame{MsgType::Reveal, 42, static_cast<uint8_t>(Move::Scissors)}, buf);
    Frame out{};
    CHECK(feed_all(p, buf, kFrameLen, out));
    CHECK(out.type == MsgType::Reveal && out.round == 42 && out.payload == 2);
}

static void test_noise_before_sync_is_ignored() {
    FrameParser p;
    uint8_t buf[kFrameLen + 3] = {0x00, 0x13, 0xFF};
    encode(Frame{MsgType::Ready, 7, 0}, &buf[3]);
    Frame out{};
    CHECK(feed_all(p, buf, sizeof(buf), out));
    CHECK(out.type == MsgType::Ready && out.round == 7);
}

static void test_corrupted_frame_rejected() {
    FrameParser p;
    uint8_t buf[kFrameLen];
    encode(Frame{MsgType::Reveal, 3, 1}, buf);
    buf[3] ^= 0x01;  // flip one bit in the payload
    Frame out{};
    CHECK(!feed_all(p, buf, kFrameLen, out));
    CHECK(p.errorCount() == 1);
}

static void test_invalid_move_rejected() {
    FrameParser p;
    uint8_t buf[kFrameLen];
    encode(Frame{MsgType::Reveal, 3, 9}, buf);  // 9 is not a legal move, CRC is valid
    Frame out{};
    CHECK(!feed_all(p, buf, kFrameLen, out));
}

static void test_recovers_after_bad_frame() {
    FrameParser p;
    uint8_t bad[kFrameLen], good[kFrameLen];
    encode(Frame{MsgType::Ready, 1, 0}, bad);
    bad[4] ^= 0xFF;
    encode(Frame{MsgType::Ready, 2, 0}, good);
    Frame out{};
    feed_all(p, bad, kFrameLen, out);
    CHECK(feed_all(p, good, kFrameLen, out));
    CHECK(out.round == 2);
}

static void test_resyncs_after_lost_byte() {
    // Drop the last byte of frame 1: the parser reads frame 2's sync as frame 1's CRC,
    // then must resynchronize and still decode frame 2.
    FrameParser p;
    uint8_t first[kFrameLen], second[kFrameLen];
    encode(Frame{MsgType::Ready, 5, 0}, first);
    encode(Frame{MsgType::Reveal, 5, 1}, second);
    uint8_t stream[2 * kFrameLen - 1];
    int n = 0;
    for (int i = 0; i < kFrameLen - 1; ++i) stream[n++] = first[i];  // CRC byte lost
    for (int i = 0; i < kFrameLen; ++i) stream[n++] = second[i];
    Frame out{};
    CHECK(feed_all(p, stream, n, out));
    CHECK(out.type == MsgType::Reveal && out.payload == 1);
}

int main() {
    test_rules();
    test_round_trip();
    test_noise_before_sync_is_ignored();
    test_corrupted_frame_rejected();
    test_invalid_move_rejected();
    test_recovers_after_bad_frame();
    test_resyncs_after_lost_byte();
    if (failures == 0) std::printf("All protocol tests passed.\n");
    return failures == 0 ? 0 : 1;
}
