// protocol.h - Game rules and board-to-board frame format.
// Hardware-independent so it can be unit tested on a PC (see tests/).
#pragma once
#include <stdint.h>

namespace rps {

enum class Move : uint8_t { Rock = 0, Paper = 1, Scissors = 2, None = 0xFF };
enum class Outcome : uint8_t { Win, Lose, Tie };

Outcome judge(Move mine, Move theirs);
const char* moveName(Move m);
Move nextMove(Move m);

// Frame on the wire: [SYNC][TYPE][ROUND][PAYLOAD][CRC8]
enum class MsgType : uint8_t { Ready = 0x01, Reveal = 0x02 };

struct Frame {
    MsgType type;
    uint8_t round;    // which round this message belongs to
    uint8_t payload;  // Reveal: the move. Ready: unused (0).
};

constexpr uint8_t kSync = 0xA5;
constexpr uint8_t kFrameLen = 5;

uint8_t crc8(const uint8_t* data, uint8_t len);
void encode(const Frame& f, uint8_t out[kFrameLen]);

// Byte-at-a-time parser: feed it every received byte and it reports
// complete, checksum-valid frames. Garbage and corrupted frames are dropped.
class FrameParser {
public:
    bool feed(uint8_t byte, Frame& out);
    uint16_t errorCount() const { return errors_; }

private:
    uint8_t buf_[kFrameLen - 1] = {};
    uint8_t idx_ = 0;
    bool inFrame_ = false;
    uint16_t errors_ = 0;
};

}  // namespace rps
