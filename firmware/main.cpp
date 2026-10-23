// main.cpp - Two-board rock-paper-scissors, bare-metal on the ATmega328P.
//
// Both boards run this same firmware; there is no master/slave. Each board:
//   1. lets the player cycle through moves and lock one in,
//   2. announces READY (without revealing the move),
//   3. reveals its move only after the opponent has also locked in,
//   4. scores the round once both moves are known.
// Every message carries the round number and is re-sent every 100 ms until
// the game moves on, so a dropped or corrupted byte only causes a short delay.
// If one board moves to the next round before the other has received its
// move, it keeps re-sending the previous round's move until the other board
// catches up, so the link can't deadlock.
#include <avr/interrupt.h>
#include <avr/io.h>

#include "lcd_i2c.h"
#include "protocol.h"
#include "timebase.h"
#include "uart.h"

using namespace rps;

namespace {
constexpr uint32_t kBaud = 9600;
constexpr uint8_t kLcdAddress = 0x27;
constexpr uint32_t kResendMs = 100;
constexpr uint8_t kPointsToWin = 3;

enum class State : uint8_t { Choosing, WaitingForPeer, Revealing, ShowResult, MatchOver };

struct Game {
    State state = State::Choosing;
    uint8_t round = 0;
    Move myMove = Move::Rock;
    Move prevMove = Move::None;  // our move from the previous round
    bool peerCaughtUp = true;    // heard from the peer in the current round yet?
    Move peerMove = Move::None;
    bool peerReady = false;
    uint8_t myScore = 0;
    uint8_t peerScore = 0;
    Outcome lastOutcome = Outcome::Tie;
    bool screenDirty = true;
};

Game game;
FrameParser parser;
uint32_t lastSendMs = 0;

// ---- Link layer ----
void send_frame(MsgType type, uint8_t round, uint8_t payload) {
    uint8_t bytes[kFrameLen];
    encode(Frame{type, round, payload}, bytes);
    for (uint8_t b : bytes) uart_write(b);
}

void on_frame(const Frame& f) {
    PINB = _BV(PB5);  // toggle the on-board LED: visible link activity

    if (f.round != game.round) return;  // stale message from another round
    game.peerCaughtUp = true;

    game.peerReady = true;  // READY and REVEAL both mean "opponent has locked in"
    if (f.type == MsgType::Reveal && game.peerMove == Move::None) {
        game.peerMove = static_cast<Move>(f.payload);
    }
    game.screenDirty = true;
}

void poll_link() {
    uint8_t byte;
    Frame f;
    while (uart_read(byte)) {
        if (parser.feed(byte, f)) on_frame(f);
    }
}

void periodic_send(uint32_t now) {
    if (now - lastSendMs < kResendMs) return;
    lastSendMs = now;

    // If we moved to a new round before the peer confirmed it has our last move,
    // keep re-sending that move so a peer stuck on the old round can finish it.
    if (!game.peerCaughtUp && game.prevMove != Move::None) {
        send_frame(MsgType::Reveal, static_cast<uint8_t>(game.round - 1),
                   static_cast<uint8_t>(game.prevMove));
    }
    switch (game.state) {
        case State::WaitingForPeer:
            send_frame(MsgType::Ready, game.round, 0);  // never reveals the move
            break;
        case State::Revealing:
        case State::ShowResult:  // keep revealing in case the peer missed it
        case State::MatchOver:
            send_frame(MsgType::Reveal, game.round, static_cast<uint8_t>(game.myMove));
            break;
        default:
            break;
    }
}

// ---- Small string helpers (avoids pulling printf into an 8-bit build) ----
char* append(char* dst, const char* src) {
    while (*src) *dst++ = *src++;
    *dst = '\0';
    return dst;
}

char* append_score(char* dst) {
    *dst++ = static_cast<char>('0' + game.myScore);
    *dst++ = '-';
    *dst++ = static_cast<char>('0' + game.peerScore);
    *dst = '\0';
    return dst;
}

// ---- Display ----
void render() {
    if (!game.screenDirty) return;
    game.screenDirty = false;

    char top[17] = {}, bottom[17] = {};
    switch (game.state) {
        case State::Choosing:
            append(append(top, "Pick: "), moveName(game.myMove));
            append(bottom, game.peerReady ? "Opponent ready!" : "Cycle / Lock");
            break;
        case State::WaitingForPeer:
            append(append(top, "Locked: "), moveName(game.myMove));
            append(bottom, "Waiting...");
            break;
        case State::Revealing:
            append(top, "Revealing...");
            break;
        case State::ShowResult: {
            const char* verdict = game.lastOutcome == Outcome::Win    ? "YOU WIN "
                                : game.lastOutcome == Outcome::Lose   ? "YOU LOSE "
                                                                      : "TIE ";
            append(append(top, "Opp: "), moveName(game.peerMove));
            append_score(append(bottom, verdict));
            break;
        }
        case State::MatchOver:
            append(top, game.myScore > game.peerScore ? "MATCH WON!" : "MATCH LOST");
            append(append_score(append(bottom, "Final ")), " Lock=new");
            break;
    }
    lcd_print_line(0, top);
    lcd_print_line(1, bottom);
}

// ---- Game state machine ----
void start_round(bool newMatch) {
    if (newMatch) { game.myScore = 0; game.peerScore = 0; }
    game.prevMove = game.myMove;
    game.peerCaughtUp = false;
    ++game.round;
    game.peerMove = Move::None;
    game.peerReady = false;
    game.state = State::Choosing;
    game.screenDirty = true;
}

void update_game() {
    switch (game.state) {
        case State::Choosing:
            if (button_pressed(kButtonCycle)) {
                game.myMove = nextMove(game.myMove);
                game.screenDirty = true;
            }
            if (button_pressed(kButtonLock)) {
                game.state = State::WaitingForPeer;
                lastSendMs = 0;  // announce immediately
                game.screenDirty = true;
            }
            break;

        case State::WaitingForPeer:
            if (game.peerReady) {
                game.state = State::Revealing;
                lastSendMs = 0;
                game.screenDirty = true;
            }
            break;

        case State::Revealing:
            if (game.peerMove != Move::None) {
                game.lastOutcome = judge(game.myMove, game.peerMove);
                if (game.lastOutcome == Outcome::Win) ++game.myScore;
                if (game.lastOutcome == Outcome::Lose) ++game.peerScore;
                game.state = State::ShowResult;
                game.screenDirty = true;
            }
            break;

        case State::ShowResult:
            if (button_pressed(kButtonLock)) {
                if (game.myScore >= kPointsToWin || game.peerScore >= kPointsToWin) {
                    game.state = State::MatchOver;
                    game.screenDirty = true;
                } else {
                    start_round(false);
                }
            }
            break;

        case State::MatchOver:
            if (button_pressed(kButtonLock)) start_round(true);
            break;
    }
    button_pressed(kButtonCycle);  // discard presses that don't apply in this state
}
}  // namespace

int main() {
    DDRB |= _BV(PB5);  // on-board LED (pin 13) as output
    uart_init(kBaud);
    timebase_init();
    sei();
    lcd_init(kLcdAddress);

    for (;;) {
        const uint32_t now = millis();
        poll_link();
        update_game();
        periodic_send(now);
        render();
    }
}
